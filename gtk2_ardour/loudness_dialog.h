/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/box.h>
#include <gtkmm/expander.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

#include "ardour_dialog.h"
#include "progress_reporter.h"

namespace ARDOUR {
	class TimelineRange;
	class ExportStatus;
	class PluginInsert;
	class Processor;
	class Session;
}

class LoudnessDialog : public ArdourDialog
{
public:
	LoudnessDialog (ARDOUR::Session*, ARDOUR::TimelineRange const&, bool);
	int run ();

	float gain_db () const;

protected:
	bool on_delete_event (GdkEventAny*);

private:
	int  analyze ();
	void start_analysis ();
	void cancel_analysis ();
	gint progress_timeout ();
	void display_results ();
	void display_report ();
	void calculate_gain ();

	void load_preset (size_t);
	void apply_preset ();
	void update_settings ();
	void update_sensitivity ();

	void test_conformity ();
	void toggle_conformity_display ();

	struct LoudnessPreset
	{
		std::string name;
		bool        enable[5];
		float       level[5];
		float       max_integrated[2];
	};

	static LoudnessPreset presets[];
	static LoudnessPreset _preset;

	ARDOUR::Session*                        _session;
	ARDOUR::TimelineRange const&               _range;
	boost::shared_ptr<ARDOUR::ExportStatus> _status;
	bool                                    _autostart;

	Gtk::VBox        _setup_box;
	Gtk::VBox        _progress_box;
	Gtk::VBox        _result_box;
	Gtk::Expander    _conformity_expander;
	Gtk::ProgressBar _progress_bar;
	Gtk::Button*     _ok_button;
	Gtk::Button*     _cancel_button;

	ArdourWidgets::ArdourButton _dbfs_btn;
	ArdourWidgets::ArdourButton _dbtp_btn;
	ArdourWidgets::ArdourButton _lufs_i_btn;
	ArdourWidgets::ArdourButton _lufs_s_btn;
	ArdourWidgets::ArdourButton _lufs_m_btn;

	Gtk::Label       _dbfs_label;
	Gtk::Label       _dbtp_label;
	Gtk::Label       _lufs_i_label;
	Gtk::Label       _lufs_s_label;
	Gtk::Label       _lufs_m_label;

	Gtk::Label       _delta_dbfs_label;
	Gtk::Label       _delta_dbtp_label;
	Gtk::Label       _delta_lufs_i_label;
	Gtk::Label       _delta_lufs_s_label;
	Gtk::Label       _delta_lufs_m_label;

	Gtk::Label       _gain_out_label;
	Gtk::Label       _gain_amp_label;
	Gtk::Label       _gain_norm_label;
	Gtk::Label       _gain_total_label;
	Gtk::Label       _gain_exceeds_label;

	ArdourWidgets::ArdourButton _rt_analysis_button;
	ArdourWidgets::ArdourButton _start_analysis_button;
	ArdourWidgets::ArdourButton _show_report_button;
	ArdourWidgets::ArdourButton _custom_pos_button;

	ArdourWidgets::ArdourDropdown _preset_dropdown;
	std::string                   _initial_preset_name;

  Gtk::Adjustment _dbfs_adjustment;
  Gtk::Adjustment _dbtp_adjustment;
  Gtk::Adjustment _lufs_i_adjustment;
  Gtk::Adjustment _lufs_s_adjustment;
  Gtk::Adjustment _lufs_m_adjustment;

	Gtk::SpinButton _dbfs_spinbutton;
  Gtk::SpinButton _dbtp_spinbutton;
  Gtk::SpinButton _lufs_i_spinbutton;
  Gtk::SpinButton _lufs_s_spinbutton;
  Gtk::SpinButton _lufs_m_spinbutton;

	float _dbfs;
	float _dbtp;
	float _lufs_i;
	float _lufs_s;
	float _lufs_m;

	float _gain_out;
	float _gain_norm;
	bool  _ignore_preset;
	bool  _ignore_change;
};
