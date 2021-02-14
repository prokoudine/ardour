/*
 * Copyright (C) 2017-2018 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef TEMPORAL_BEATS_HPP
#define TEMPORAL_BEATS_HPP

#include <cassert>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include <iostream>
#include <limits>

#include "pbd/compose.h"
#include "pbd/failed_constructor.h"
#include "pbd/string_convert.h"


#include "temporal/visibility.h"
#include "temporal/types.h"

namespace ARDOUR {
class Variant; /* Can stay since LV2 has no way to exchange beats as anything except double */
/* these all need fixing to not use ::to_double() */
class TempoMap;
class Track;
class MidiStretch;
class MidiModel;
class AutomationList;
class MidiSource;
class MidiRegion;
/* these use ::to_double() but should be removed */
class DoubleBeatsSamplesConverter;
}

namespace Evoral {
template<typename T> class Sequence;
}

/* XXX hack friends for ::do_double() access ... remove */

class QuantizeDialog;
class NoteDrag;
class NoteCreateDrag;

namespace Temporal {

/** Musical time in beats. */
class /*LIBTEMPORAL_API*/ Beats {
public:
	LIBTEMPORAL_API static const int32_t PPQN = Temporal::ticks_per_beat;

	Beats() : _beats(0), _ticks(0) {}
	Beats(const Beats& other) : _beats(other._beats), _ticks(other._ticks) {}

	/** Normalize so ticks is within PPQN. */
	void normalize() {
		// First, fix negative ticks with positive beats
		while (_beats > 0 && _ticks < 0) {
			--_beats;
			_ticks += PPQN;
		}

		// Now fix positive ticks with negative beats
		while (_beats < 0 && _ticks > 0) {
			++_beats;
			_ticks -= PPQN;
		}

		assert ((_beats < 0 && _ticks <= 0) || (_beats > 0 && _ticks >= 0) || _beats == 0);

		// Work with positive beats and ticks to normalize
		const int32_t sign  = _beats < 0 ? -1 : _ticks < 0 ? -1 : 1;
		int32_t       beats = abs(_beats);
		int32_t       ticks = abs(_ticks);

		// Fix ticks greater than 1 beat
		while (ticks >= PPQN) {
			++beats;
			ticks -= PPQN;
		}

		// Set fields with appropriate sign
		_beats = sign * beats;
		_ticks = sign * ticks;
	}

	/** Create from a precise beats:ticks pair. */
	explicit Beats(int32_t b, int32_t t) : _beats(b), _ticks(t) {
		normalize();
	}

	/** Create from a real number of beats. */
	static Beats from_double (double beats) {
		double       whole;
		const double frac = modf (beats, &whole);
		return Beats (whole, (int32_t) rint (frac * PPQN));
	}

	/** Create from an integer number of beats. */
	static Beats beats(int32_t beats) {
		return Beats(beats, 0);
	}

	/** Create from ticks at the standard PPQN. */
	static Beats ticks(int32_t ticks) {
		return Beats(0, ticks);
	}

	/** Create from ticks at a given rate.
	 *
	 * Note this can also be used to create from frames by setting ppqn to the
	 * number of samples per beat.  Note the resulting Beats will, like all
	 * others, have the default PPQN, so this is a potentially lossy
	 * conversion.
	 */
	static Beats ticks_at_rate(int64_t ticks, uint32_t ppqn) {
		return Beats(ticks / ppqn, (ticks % ppqn) * PPQN / ppqn);
	}

	static int64_t make_ticks (Beats const & b) { return b.get_beats() * ticks_per_beat + b.get_ticks(); }

	int64_t to_ticks()               const { return (int64_t)_beats * PPQN + _ticks; }
	int64_t to_ticks(uint32_t ppqn)  const { return (int64_t)_beats * ppqn + (_ticks * ppqn / PPQN); }

	int32_t get_beats() const { return _beats; }
	int32_t get_ticks() const { return _ticks; }

	Beats& operator=(double time) {
		double       whole;
		const double frac = modf(time, &whole);

		_beats = whole;
		_ticks = frac * PPQN;
		return *this;
	}

	Beats& operator=(const Beats& other) {
		_beats = other._beats;
		_ticks = other._ticks;
		return *this;
	}

	Beats round_to_beat() const {
		return (_ticks >= (PPQN/2)) ? Beats (_beats + 1, 0) : Beats (_beats, 0);
	}

	Beats round_up_to_beat() const {
		return (_ticks == 0) ? *this : Beats(_beats + 1, 0);
	}

	Beats round_down_to_beat() const {
		return Beats(_beats, 0);
	}


	Beats prev_beat() const {
		/* always moves backwards even if currently on beat */
		return Beats (_beats-1, 0);
	}

	Beats next_beat() const {
		/* always moves forwards even if currently on beat */
		return Beats (_beats+1, 0);
	}

	Beats round_to_subdivision (int subdivision, RoundMode dir) const {
		uint32_t ticks = to_ticks();
		const uint32_t ticks_one_subdivisions_worth = ticks_per_beat / subdivision;
		uint32_t mod = ticks % ticks_one_subdivisions_worth;
		uint32_t beats = _beats;

		if (dir > 0) {

			if (mod == 0 && dir == RoundUpMaybe) {
				/* right on the subdivision, which is fine, so do nothing */

			} else if (mod == 0) {
				/* right on the subdivision, so the difference is just the subdivision ticks */
				ticks += ticks_one_subdivisions_worth;

			} else {
				/* not on subdivision, compute distance to next subdivision */

				ticks += ticks_one_subdivisions_worth - mod;
			}

			// NOTE:  this code intentionally limits the rounding so we don't advance to the next beat.
			// For the purposes of "jump-to-next-subdivision", we DO want to advance to the next beat.
			// And since the "prev" direction DOES move beats, I assume this code is unintended.
			// But I'm keeping it around, until we determine there are no terrible consequences.
			// if (ticks >= BBT_Time::ticks_per_beat) {
			//	ticks -= BBT_Time::ticks_per_beat;
			// }

		} else if (dir < 0) {

			/* round to previous (or same iff dir == RoundDownMaybe) */

			uint32_t difference = ticks % ticks_one_subdivisions_worth;

			if (difference == 0 && dir == RoundDownAlways) {
				/* right on the subdivision, but force-rounding down,
				   so the difference is just the subdivision ticks */
				difference = ticks_one_subdivisions_worth;
			}

			if (ticks < difference) {
				ticks = ticks_per_beat - ticks;
			} else {
				ticks -= difference;
			}

		} else {
			/* round to nearest */
			double rem;

			/* compute the distance to the previous and next subdivision */

			if ((rem = fmod ((double) ticks, (double) ticks_one_subdivisions_worth)) > ticks_one_subdivisions_worth/2.0) {

				/* closer to the next subdivision, so shift forward */

				ticks = lrint (ticks + (ticks_one_subdivisions_worth - rem));

				//DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved forward to %1\n", ticks));

				if (ticks > ticks_per_beat) {
					++beats;
					ticks -= ticks_per_beat;
					//DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("fold beat to %1\n", beats));
				}

			} else if (rem > 0) {

				/* closer to previous subdivision, so shift backward */

				if (rem > ticks) {
					if (beats == 0) {
						/* can't go backwards past zero, so ... */
						return *this;
					}
					/* step back to previous beat */
					--beats;
					ticks = lrint (ticks_per_beat - rem);
					//DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("step back beat to %1\n", beats));
				} else {
					ticks = lrint (ticks - rem);
					//DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved backward to %1\n", ticks));
				}
			} else {
				/* on the subdivision, do nothing */
			}
		}

		return Beats::ticks (ticks);
	}

	Beats snap_to (Temporal::Beats const & snap) const {
		const double snap_time = snap.to_double();
		return Beats::from_double (ceil(to_double() / snap_time) * snap_time);
	}

	Beats abs () const {
		return Beats (::abs (_beats), ::abs (_ticks));
	}

	Beats diff (Beats const & other) const {
		if (other > *this) {
			return other - *this;
		}
		return *this - other;
	}

	inline bool operator==(const Beats& b) const {
		return _beats == b._beats && _ticks == b._ticks;
	}

	inline bool operator==(double t) const {
		/* Acceptable tolerance is 1 tick. */
		return fabs(to_double() - t) <= (1.0 / PPQN);
	}

	inline bool operator==(int beats) const {
		return _beats == beats;
	}

	inline bool operator!=(const Beats& b) const {
		return !operator==(b);
	}

	inline bool operator<(const Beats& b) const {
		return _beats < b._beats || (_beats == b._beats && _ticks < b._ticks);
	}

	inline bool operator<=(const Beats& b) const {
		return _beats < b._beats || (_beats == b._beats && _ticks <= b._ticks);
	}

	inline bool operator>(const Beats& b) const {
		return _beats > b._beats || (_beats == b._beats && _ticks > b._ticks);
	}

	inline bool operator>=(const Beats& b) const {
		return _beats > b._beats || (_beats == b._beats && _ticks >= b._ticks);
	}

	inline bool operator<(double b) const {
		/* Acceptable tolerance is 1 tick. */
		const double time = to_double();
		if (fabs(time - b) <= (1.0 / PPQN)) {
			return false;  /* Effectively identical. */
		} else {
			return time < b;
		}
	}

	inline bool operator<=(double b) const {
		return operator==(b) || operator<(b);
	}

	inline bool operator>(double b) const {
		/* Acceptable tolerance is 1 tick. */
		const double time = to_double();
		if (fabs(time - b) <= (1.0 / PPQN)) {
			return false;  /* Effectively identical. */
		} else {
			return time > b;
		}
	}

	inline bool operator>=(double b) const {
		return operator==(b) || operator>(b);
	}

	Beats operator+(const Beats& b) const {
		return Beats(_beats + b._beats, _ticks + b._ticks);
	}

	Beats operator-(const Beats& b) const {
		return Beats(_beats - b._beats, _ticks - b._ticks);
	}

	Beats operator+(double d) const {
		return Beats(to_double() + d);
	}

	Beats operator-(double d) const {
		return Beats(to_double() - d);
	}

	Beats operator+(int b) const {
		return Beats (_beats + b, _ticks);
	}

	Beats operator-(int b) const {
		return Beats (_beats - b, _ticks);
	}

	Beats& operator+=(int b) {
		_beats += b;
		return *this;
	}

	Beats& operator-=(int b) {
		_beats -= b;
		return *this;
	}

	Beats operator-() const {
		/* must avoid normalization here, which will convert a negative
		   value into a valid beat position before zero, which is not
		   we want here.
		*/
		Beats b (_beats, _ticks);
		b._beats = -b._beats;
		b._ticks = -b._ticks;
		return b;
	}

	template<typename Number>
	Beats operator*(Number factor) const {
		return ticks ((_beats * PPQN + _ticks) * factor);
	}

	template<typename Number>
	Beats operator/(Number factor) const {
		return ticks ((_beats * PPQN + _ticks) / factor);
	}

	Beats operator% (Beats const & b) {
		return Beats::ticks (to_ticks() % b.to_ticks());
	}

	/* avoids calling ::to_double() to compute ratios of two Beat distances
	 */
	double operator/ (Beats const & other) {
		return (double) to_ticks() / (double) other.to_ticks();
	}

	Beats& operator+=(const Beats& b) {
		_beats += b._beats;
		_ticks += b._ticks;
		normalize();
		return *this;
	}

	Beats& operator-=(const Beats& b) {
		_beats -= b._beats;
		_ticks -= b._ticks;
		normalize();
		return *this;
	}

	bool operator!() const { return _beats == 0 && _ticks == 0; }
	operator bool () const { return _beats != 0 || _ticks != 0; }

	static Beats one_tick() { return Beats(0, 1); }

private:
	int32_t _beats;
	int32_t _ticks;

	/* almost nobody should ever be allowed to use this method */
	friend class TempoPoint;
	friend class ARDOUR::TempoMap;
	friend class ARDOUR::Track;
	friend class ARDOUR::Variant;
	friend class ARDOUR::MidiStretch;
	friend class ARDOUR::MidiModel;
	friend class ARDOUR::AutomationList;
	friend class ARDOUR::MidiSource;
	friend class ARDOUR::MidiRegion;
	friend class ARDOUR::DoubleBeatsSamplesConverter;
	friend class ::QuantizeDialog;
	friend class ::NoteDrag;
	friend class ::NoteCreateDrag;
	double  to_double()              const { return (double)_beats + (_ticks / (double)PPQN); }

	/* this needs to exist because Evoral::Sequence is templated, and some
	 * other possible template types cannot provide ::from_double
	 */

	friend class Evoral::Sequence<Beats>;
	explicit Beats (double beats) {
		double       whole;
		const double frac = modf (beats, &whole);

		_beats = whole;
		_ticks = frac * PPQN;
	}
};

/*
  TIL, several horrible hours later, that sometimes the compiler looks in the
  namespace of a type (Temporal::Beats in this case) for an operator, and
  does *NOT* look in the global namespace.

  C++ is proof that hell exists and we are living in it.  In any case, move
  these to the global namespace and PBD::Property's loopy
  virtual-method-in-a-template will bite you.
*/

inline std::ostream&
operator<<(std::ostream& os, const Beats& t)
{
	os << t.get_beats() << ':' << t.get_ticks();
	return os;
}

inline std::istream&
operator>>(std::istream& istr, Beats& b)
{
	int32_t beats, ticks;
	char d; /* delimiter, whatever it is */
	istr >> beats >> d >> ticks;
	b = Beats (beats, ticks);
	return istr;
}

} // namespace Temporal

namespace PBD {
	namespace DEBUG {
		LIBTEMPORAL_API extern uint64_t Beats;
	}
}

namespace std {
	template<>
	struct numeric_limits<Temporal::Beats> {
		static Temporal::Beats lowest() {
			return Temporal::Beats(std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min());
		}

		/* We don't define min() since this has different behaviour for integral and floating point types,
		   but Beats is used as both.  Better to avoid providing a min at all
		   than a confusing one. */

		static Temporal::Beats max() {
			return Temporal::Beats(std::numeric_limits<int32_t>::max(), Temporal::Beats::PPQN-1);
		}
	};
}

namespace PBD {
	namespace DEBUG {
		LIBTEMPORAL_API extern uint64_t Beats;
	}

template<>
inline bool to_string (Temporal::Beats val, std::string & str)
{
	std::ostringstream ostr;
	ostr << val;
	str = ostr.str();
	return true;
}

template<>
inline bool string_to (std::string const & str, Temporal::Beats & val)
{
	std::istringstream istr (str);
	istr >> val;
	return (bool) istr;
}

} /* end namsepace PBD */


#endif // TEMPORAL_BEATS_HPP
