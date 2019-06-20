///////////////////////////////////////////////////////////////////////////////////
//
//                                  ~~  ~~             ~~
//  ~~~~~~    ~~~~~    ~~~~~    ~~~~~~  ~~     ~~~~~   ~~~~~~    ~~~~~   ~~    ~~
//  ~~   ~~  ~~   ~~  ~~   ~~  ~~   ~~  ~~    ~~   ~~  ~~   ~~  ~~   ~~   ~~  ~~
//  ~~   ~~  ~~   ~~  ~~   ~~  ~~   ~~  ~~    ~~~~~~~  ~~   ~~  ~~   ~~     ~~
//  ~~   ~~  ~~   ~~  ~~   ~~  ~~   ~~  ~~    ~~       ~~   ~~  ~~   ~~   ~~  ~~
//  ~~   ~~   ~~~~~    ~~~~~    ~~~~~~   ~~~   ~~~~~   ~~~~~~    ~~~~~   ~~    ~~
//
//  Serendipity Sequencer                                   CC-NC-BY-SA
//  hotchk155/2018                                          Sixty-four pixels ltd
//
//  SEQUENCER LAYER
//
///////////////////////////////////////////////////////////////////////////////////
#ifndef SEQUENCE_LAYER_H_
#define SEQUENCE_LAYER_H_

///////////////////////////////////////////////////////////////////////////////
// This class holds all the info for a single layer/part
class CSequenceLayer {

public:
	typedef enum {
		VIEW_PITCH,
		VIEW_PITCH_OFFSET,
		VIEW_MODULATION
	} VIEW_TYPE;

	enum {
		OFFSET_ZERO = 64,		// step value for zero transpose offset
		MAX_PAGES = 4					// number of pages
	};

private:

	enum {
		MAX_PLAYING_NOTES = 8,
		DEFAULT_SCROLL_OFS = 24,
		MAX_PAGE_LIST = 16
	};

	// This structure holds the layer information that gets saved with the patch
	typedef struct {
		CSequencePage 		m_page[MAX_PAGES];	// sequencer page
		byte 			m_page_list[MAX_PAGE_LIST];
		byte			m_page_list_count;
		V_SQL_SEQ_MODE 	m_mode;				// the mode for this layer (note, mod etc)
		V_SQL_FORCE_SCALE	m_force_scale;	// force to scale
		V_SQL_STEP_RATE m_step_rate;		// step rate setting
		char			m_transpose;		// manual transpose amount for the layer
		V_SQL_NOTE_DUR	m_note_dur;
		V_SQL_MIDI_CHAN m_midi_channel;		// MIDI channel
		byte 			m_midi_cc;			// MIDI CC
		V_SQL_CVSCALE	m_cv_scale;
		byte 			m_cv_range;
		V_SQL_CVGLIDE	m_cv_glide;
		V_SQL_TRAN_TRIG	m_tran_trig;
		V_SQL_TRAN_ACC	m_tran_acc;
		byte 			m_midi_vel_accent;
		byte 			m_midi_vel;
		byte 			m_max_page_no;		// the highest numbered active page (0-3)
		byte 			m_loop_per_page :1;
		int				m_interpolate:1;
		int 			m_enabled:1;
		int 			m_page_advance:1;
	} CONFIG;



	typedef struct {
		VIEW_TYPE m_view;
		byte m_scroll_ofs;					// lowest step value shown on grid
		int m_play_page_no;				// the page number being played
		int m_play_pos;
		int m_page_list_pos;


		CSequenceStep m_step_value;			// the last value output by sequencer
		byte m_stepped;						// stepped flag
		byte m_page_advanced;
		byte m_midi_note; 					// last midi note played on channel
		uint32_t m_next_tick;
		byte m_last_tick_lsb;
		uint32_t m_gate_timeout;
	} STATE;

	const uint32_t INFINITE_GATE = (uint32_t)(-1);

	CONFIG m_cfg;				// instance of config
	STATE m_state;

	//
	// PRIVATE METHODS
	//


	///////////////////////////////////////////////////////////////////////////////
	// accessor for a page
	inline CSequencePage& get_page(byte page_no) {
		ASSERT(page_no >= 0 && page_no < MAX_PAGES);
		return m_cfg.m_page[page_no];
	}

	///////////////////////////////////////////////////////////////////////////////
	byte get_default_value() {
		switch(m_cfg.m_mode) {
			case V_SQL_SEQ_MODE_TRANSPOSE:
				return OFFSET_ZERO;
			case V_SQL_SEQ_MODE_PITCH:
				return g_scale.default_note();
			case V_SQL_SEQ_MODE_MOD:
			default:
				return 0;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	// Calculate the page and step
	void calc_next_step(int &page_no, int &step_no, byte& page_switch, int &page_list_pos) {

		CSequencePage& page = get_page(page_no);
		page_switch=0;
		if(step_no == page.get_loop_to()) {

			if(m_cfg.m_page_advance) { // automatic advance at end of page?
				if(m_cfg.m_page_list_count) { // has user got a page list?
					if(page_list_pos < m_cfg.m_page_list_count - 1) {
						++page_list_pos;
					}
					else {
						page_list_pos = 0;
					}
					byte next_page_no = m_cfg.m_page_list[page_list_pos];
					if(next_page_no <= m_cfg.m_max_page_no) {
						// still a valid page
						page_no = next_page_no;
						page_switch = 1;
					}
				}
				else {
					if(page_no < m_cfg.m_max_page_no) {
						// automatic page advance to the next page
						++page_no;
						page_switch = 1;
					}
					else {
						// reached end of last page, going back to first
						page_no = 0;
						page_switch = 1;
					}
				}
			}
			// back to first step
			CSequencePage& new_page = get_page(page_no); // could be same page
			step_no = new_page.get_loop_from();
		}
		else {
			if(page.get_loop_to() < page.get_loop_from()) { // run backwards
				if(--step_no < 0) {
					step_no = CSequencePage::MAX_STEPS-1;
				}
				else if(step_no > page.get_loop_from()) {
					step_no = page.get_loop_from();
				}
				else if(step_no < page.get_loop_to()) {
					step_no = page.get_loop_to();
				}
			}
			else {
				if(++step_no > CSequencePage::MAX_STEPS-1) {
					step_no = 0;
				}
				else if(step_no > page.get_loop_to()) {
					step_no = page.get_loop_to();
				}
				else if(step_no < page.get_loop_from()) {
					step_no = page.get_loop_from();
				}
			}
		}
	}


	void recalc_data_points_all_pages() {
		for(int i=0; i<MAX_PAGES; ++i) {
			CSequencePage& page = get_page(i);
			page.recalc(m_cfg.m_interpolate, get_default_value());
		}
	}


public:
	///////////////////////////////////////////////////////////////////////////////
	void clear_data_point(byte page_no, byte index) {
		CSequencePage& page = get_page(page_no);
		CSequenceStep step = page.get_step(index);
		step.clear();
		page.set_step(index, step, m_cfg.m_interpolate, get_default_value());
	}

	///////////////////////////////////////////////////////////////////////////////
	void init() {
		init_config();
		init_state();
	}
	///////////////////////////////////////////////////////////////////////////////
	inline CScale& get_scale() {
		return g_scale;
	}

	///////////////////////////////////////////////////////////////////////////////
	// Assign valid default values to the sequence layer configuration (i.e.
	// the data that forms part of a saved sequence)
	void init_config() {
		m_cfg.m_mode 		= V_SQL_SEQ_MODE_PITCH;
		m_cfg.m_force_scale = V_SQL_FORCE_SCALE_OFF;
		m_cfg.m_step_rate	= V_SQL_STEP_RATE_16;
		m_cfg.m_note_dur	= V_SQL_NOTE_DUR_100;
//		m_cfg.m_loop_from	= 0;
//		m_cfg.m_loop_to		= 15;
		m_cfg.m_transpose	= 0;
		m_cfg.m_midi_channel 	= V_SQL_MIDI_CHAN_NONE;
		m_cfg.m_midi_cc = 1;
		m_cfg.m_enabled = 1;
		m_cfg.m_cv_scale = V_SQL_CVSCALE_1VOCT;
		m_cfg.m_cv_range = 5;
		m_cfg.m_cv_glide = V_SQL_CVGLIDE_OFF;
		m_cfg.m_tran_trig = V_SQL_TRAN_TRIG_ORIG;
		m_cfg.m_tran_acc = V_SQL_TRAN_ACC_ACCENT;
		m_cfg.m_midi_vel_accent = 127;
		m_cfg.m_midi_vel = 100;
		m_cfg.m_interpolate = 0;
		m_cfg.m_max_page_no = 0;
		m_cfg.m_page_advance = 0;
		m_cfg.m_loop_per_page = 0;
		m_cfg.m_page_list_count = 0;
		set_mode(m_cfg.m_mode);
		clear();
	}

	///////////////////////////////////////////////////////////////////////////////
	// Initialise the state of a configured sequence layer (e.g. when the layer
	// has been loaded from EEPROM)
	void init_state() {
		m_state.m_scroll_ofs = DEFAULT_SCROLL_OFS;
		m_state.m_last_tick_lsb = 0;
		m_state.m_midi_note = 0;
		m_state.m_view = VIEW_PITCH;
		//m_state.m_next_page_no = -1;
		m_state.m_page_list_pos = 0;
		reset();
	}


	///////////////////////////////////////////////////////////////////////////////
	// Reset the playback state of the layer
	void reset() {
		m_state.m_step_value.clear();
		m_state.m_stepped = 0;
		m_state.m_play_pos = 0;
		m_state.m_next_tick = 0;
		m_state.m_gate_timeout = 0;
		m_state.m_play_page_no = 0;
		m_state.m_page_advanced = 0;
	}

	//
	// CONFIG ACCESSORS
	//

	///////////////////////////////////////////////////////////////////////////////
	void set(PARAM_ID param, int value) {
		switch(param) {
		case P_SQL_SEQ_MODE: set_mode((V_SQL_SEQ_MODE)value); break;
		case P_SQL_FORCE_SCALE: m_cfg.m_force_scale = (V_SQL_FORCE_SCALE)value; break;
		case P_SQL_STEP_RATE: m_cfg.m_step_rate = (V_SQL_STEP_RATE)value; break;
		case P_SQL_NOTE_DUR: m_cfg.m_note_dur = (V_SQL_NOTE_DUR)value; break;
		case P_SQL_MIDI_CHAN: m_cfg.m_midi_channel = (V_SQL_MIDI_CHAN)value; break;
		case P_SQL_MIDI_CC: m_cfg.m_midi_cc = value; break;
		case P_SQL_CVSCALE: m_cfg.m_cv_scale = (V_SQL_CVSCALE)value; break;
		case P_SQL_CVRANGE: m_cfg.m_cv_range = value; break;
		case P_SQL_CVGLIDE: m_cfg.m_cv_glide = (V_SQL_CVGLIDE)value; break;
		case P_SQL_TRAN_TRIG: m_cfg.m_tran_trig = (V_SQL_TRAN_TRIG)value; break;
		case P_SQL_TRAN_ACC: m_cfg.m_tran_acc = (V_SQL_TRAN_ACC)value; break;
		case P_SQL_MIDI_VEL_ACCENT: m_cfg.m_midi_vel_accent = value; break;
		case P_SQL_MIDI_VEL: m_cfg.m_midi_vel = value; break;
		case P_SQL_INTERPOLATE: m_cfg.m_interpolate = value; recalc_data_points_all_pages(); break;
		case P_SQL_SCALE_TYPE: g_scale.build((V_SQL_SCALE_TYPE)value, g_scale.get_root()); break;
		case P_SQL_SCALE_ROOT: g_scale.build(g_scale.get_type(), (V_SQL_SCALE_ROOT)value); break;
		case P_SQL_LOOP_PER_PAGE: m_cfg.m_loop_per_page = value; break;
		case P_SQL_AUTO_PAGE_ADVANCE: m_cfg.m_page_advance = value; break;
		default: break;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	int get(PARAM_ID param) {
		switch(param) {
		case P_SQL_SEQ_MODE: return m_cfg.m_mode;
		case P_SQL_FORCE_SCALE: return m_cfg.m_force_scale;
		case P_SQL_STEP_RATE: return m_cfg.m_step_rate;
		case P_SQL_NOTE_DUR: return m_cfg.m_note_dur;
		case P_SQL_MIDI_CHAN: return m_cfg.m_midi_channel;
		case P_SQL_MIDI_CC: return m_cfg.m_midi_cc;
		case P_SQL_CVSCALE: return m_cfg.m_cv_scale;
		case P_SQL_CVRANGE: return m_cfg.m_cv_range;
		case P_SQL_CVGLIDE: return m_cfg.m_cv_glide;
		case P_SQL_TRAN_TRIG: return m_cfg.m_tran_trig;
		case P_SQL_TRAN_ACC: return m_cfg.m_tran_acc;
		case P_SQL_MIDI_VEL_ACCENT: return m_cfg.m_midi_vel_accent;
		case P_SQL_MIDI_VEL: return m_cfg.m_midi_vel;
		case P_SQL_INTERPOLATE: return m_cfg.m_interpolate;
		case P_SQL_SCALE_TYPE: return g_scale.get_type();
		case P_SQL_SCALE_ROOT: return g_scale.get_root();
		case P_SQL_LOOP_PER_PAGE: return m_cfg.m_loop_per_page;
		case P_SQL_AUTO_PAGE_ADVANCE: return m_cfg.m_page_advance;
		default:return 0;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	int is_valid_param(PARAM_ID param) {
		switch(param) {
		case P_SQL_FORCE_SCALE:
		case P_SQL_SCALE_TYPE:
		case P_SQL_SCALE_ROOT:
			return !!(m_cfg.m_mode == V_SQL_SEQ_MODE_PITCH||m_cfg.m_mode == V_SQL_SEQ_MODE_TRANSPOSE);
		case P_SQL_MIDI_CC:	return !!(m_cfg.m_mode == V_SQL_SEQ_MODE_MOD);
		case P_SQL_CVRANGE: return !!(m_cfg.m_mode == V_SQL_SEQ_MODE_MOD);
		case P_SQL_CVSCALE: return !(m_cfg.m_mode == V_SQL_SEQ_MODE_MOD);
		case P_SQL_TRAN_TRIG:
		case P_SQL_TRAN_ACC: return !!(m_cfg.m_mode == V_SQL_SEQ_MODE_TRANSPOSE);
		case P_SQL_MIDI_VEL:
		case P_SQL_MIDI_VEL_ACCENT:
			switch(m_cfg.m_mode) {
			case V_SQL_SEQ_MODE_PITCH:
			case V_SQL_SEQ_MODE_TRANSPOSE:
				return 1;
			case V_SQL_SEQ_MODE_MOD:
				return 0;
			default:
				break;
			}
		}
		return 1;
	}

	//
	// EDIT FUNCTIONS
	//



	///////////////////////////////////////////////////////////////////////////////
	// change the mode
	void set_mode(V_SQL_SEQ_MODE value) {
		switch (value) {
		case V_SQL_SEQ_MODE_PITCH:
			m_state.m_view = VIEW_TYPE::VIEW_PITCH;
			m_cfg.m_interpolate = 0;
			break;
		case V_SQL_SEQ_MODE_TRANSPOSE:
			m_state.m_view = VIEW_TYPE::VIEW_PITCH_OFFSET;
			m_cfg.m_interpolate = 0;
			break;
		case V_SQL_SEQ_MODE_MOD:
			m_state.m_view = VIEW_TYPE::VIEW_MODULATION;
			m_cfg.m_interpolate = 1;
			break;
		}
		m_cfg.m_mode = value;
	}

	///////////////////////////////////////////////////////////////////////////////
	CSequenceStep get_step(byte page_no, byte index) {
		CSequencePage& page = get_page(page_no);
		return page.get_step(index);
	}

	///////////////////////////////////////////////////////////////////////////////
	void set_step(byte page_no, byte index, CSequenceStep& step) {
		CSequencePage& page = get_page(page_no);
		page.set_step(index, step, m_cfg.m_interpolate, get_default_value());
	}

	///////////////////////////////////////////////////////////////////////////////
	void clear_step(byte page_no, byte index) {
		CSequencePage& page = get_page(page_no);
		page.clear_step(index, m_cfg.m_interpolate, get_default_value());
	}

	///////////////////////////////////////////////////////////////////////////////
	void clear_page(byte page_no) {
		CSequencePage& page = get_page(page_no);
		page.clear(get_default_value());
	}

	///////////////////////////////////////////////////////////////////////////////
	void clear() {
		for(int i=0; i<MAX_PAGES; ++i) {
			CSequencePage& page = get_page(i);
			page.clear(get_default_value());
		}
		m_cfg.m_max_page_no = 0;
		m_cfg.m_page_list_count = 0;
	}

	///////////////////////////////////////////////////////////////////////////////
	void shift_horizontal(byte page_no, int dir) {
		CSequencePage& page = get_page(page_no);
		page.shift_horizontal(dir);
	}

	///////////////////////////////////////////////////////////////////////////////
	byte shift_vertical(byte page_no, int dir, int scaled) {
		CSequencePage& page = get_page(page_no);
		return page.shift_vertical(dir, scaled? &g_scale : NULL, m_cfg.m_interpolate, get_default_value());
	}


	///////////////////////////////////////////////////////////////////////////////
//	inline int get_page_advance() {
	//	return m_cfg.m_page_advance;
//	}

	///////////////////////////////////////////////////////////////////////////////
	//void set_page_advance(int value) {
		//m_cfg.m_page_advance = value;
//	}

	///////////////////////////////////////////////////////////////////////////////
	inline int get_max_page_no() {
		return m_cfg.m_max_page_no;
	}

	///////////////////////////////////////////////////////////////////////////////
	// Make sure that a page is available before trying to access it. When new
	// pages are addeded, they are initialised from the last existing page
	void prepare_page(int page) {
		while(m_cfg.m_max_page_no < page) {
			get_page(m_cfg.m_max_page_no+1) = get_page(m_cfg.m_max_page_no);
			++m_cfg.m_max_page_no;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	// Change the number of existing pages
	void set_max_page_no(int page) {
		prepare_page(page);	// in case the number of pages has increased
		m_cfg.m_max_page_no = page; // in case it has got less
	}

	///////////////////////////////////////////////////////////////////////////////
	inline V_SQL_SEQ_MODE get_mode() {
		return m_cfg.m_mode;
	}

	///////////////////////////////////////////////////////////////////////////////
	inline VIEW_TYPE get_view() {
		return m_state.m_view;
	}
	///////////////////////////////////////////////////////////////////////////////
	byte get_enabled() {
		return m_cfg.m_enabled;
	}

	///////////////////////////////////////////////////////////////////////////////
	void set_enabled(byte e) {
		m_cfg.m_enabled = e;
	}

	///////////////////////////////////////////////////////////////////////////////
	int get_scroll_ofs() {
		return m_state.m_scroll_ofs;
	}

	///////////////////////////////////////////////////////////////////////////////
	void set_scroll_ofs(int scroll_ofs) {
		m_state.m_scroll_ofs = scroll_ofs;
	}
	///////////////////////////////////////////////////////////////////////////////
	void set_loop_from(byte page_no, int from) {
		if(m_cfg.m_loop_per_page) {
			get_page(page_no).set_loop_from(from);
		}
		else {
			for(int i=0; i<MAX_PAGES; ++i) {
				get_page(i).set_loop_from(from);
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	void set_loop_to(byte page_no, int to) {
		if(m_cfg.m_loop_per_page) {
			get_page(page_no).set_loop_to(to);
		}
		else {
			for(int i=0; i<MAX_PAGES; ++i) {
				get_page(i).set_loop_to(to);
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	int get_loop_from(byte page_no) {
		if(m_cfg.m_loop_per_page) {
			return get_page(page_no).get_loop_from();
		}
		else {
			return get_page(0).get_loop_from();
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	int get_loop_to(byte page_no) {
		if(m_cfg.m_loop_per_page) {
			return get_page(page_no).get_loop_to();
		}
		else {
			return get_page(0).get_loop_to();
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	void clear_page_list() {
		m_cfg.m_page_list_count = 0;
		m_state.m_page_list_pos = 0;
	}
	///////////////////////////////////////////////////////////////////////////////
	int get_page_list_count() {
		return m_cfg.m_page_list_count;
	}

	///////////////////////////////////////////////////////////////////////////////
	byte add_to_page_list(byte page_no) {
		ASSERT(page_no >= 0 && page_no < MAX_PAGES);
		if(page_no > m_cfg.m_max_page_no) {
			return 0;
		}
		if(m_cfg.m_page_list_count < MAX_PAGE_LIST) {
			m_cfg.m_page_list[m_cfg.m_page_list_count++] = page_no;
			return 1;
		}
		return 0;
	}

	///////////////////////////////////////////////////////////////////////////////
	byte is_stepped() {
		return m_state.m_stepped;
	}
	///////////////////////////////////////////////////////////////////////////////
	byte is_page_advanced() {
		return m_state.m_page_advanced;
	}
	///////////////////////////////////////////////////////////////////////////////
	CSequenceStep& get_current_step() {
		return m_state.m_step_value;
	}
	///////////////////////////////////////////////////////////////////////////////
	void set_play_page(int page_no) {
		m_state.m_play_page_no = page_no;
	}
	///////////////////////////////////////////////////////////////////////////////
	int get_play_page() {
		return m_state.m_play_page_no;
	}
	///////////////////////////////////////////////////////////////////////////////
	void set_pos(int pos) {
		m_state.m_play_pos = pos;
	}
	///////////////////////////////////////////////////////////////////////////////
	int get_pos() {
		return m_state.m_play_pos;
	}

	///////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////
	//
	// SEQUENCER FUNCTIONS
	//
	///////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////


	///////////////////////////////////////////////////////////////////////////////
	void send_midi_note(byte note, byte velocity) {
		if(m_cfg.m_midi_channel > V_SQL_MIDI_CHAN_NONE) {
			g_midi.send_note(m_cfg.m_midi_channel-V_SQL_MIDI_CHAN_1, note, velocity);
		}
	}


	///////////////////////////////////////////////////////////////////////////////
	void start(uint32_t ticks, byte parts_tick) {
		m_state.m_next_tick = ticks;

	}

	///////////////////////////////////////////////////////////////////////////////
	void stop_all_notes() {
/*		for(int i=0; i<MAX_PLAYING_NOTES;++i) {
			if(m_state.m_playing[i].count) {
				send_midi_note(m_state.m_playing[i].note, 0);
				m_state.m_playing[i].note = 0;
				m_state.m_playing[i].count = 0;
			}
		}*/
	}

	///////////////////////////////////////////////////////////////////////////////
	void tick(uint32_t ticks, byte parts_tick) {
		if(ticks >= m_state.m_next_tick) {
			m_state.m_next_tick += g_clock.ticks_per_measure(m_cfg.m_step_rate);
			calc_next_step(m_state.m_play_page_no, m_state.m_play_pos, m_state.m_page_advanced, m_state.m_page_list_pos);
			m_state.m_step_value = get_step(m_state.m_play_page_no, m_state.m_play_pos);
			m_state.m_stepped = 1;
		}
		else {
			m_state.m_stepped = 0;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	// called once per ms
	void ms_tick(int which) {
		if(m_state.m_gate_timeout) {
			if(!--m_state.m_gate_timeout) {
				g_cv_gate.gate(which, CCVGate::GATE_CLOSED);
				send_midi_note(m_state.m_midi_note, 0);
				m_state.m_midi_note = 0;
			}
		}
	}




	/*
	///////////////////////////////////////////////////////////////////////////////
	// Play a step for a note mode
	void action_step_note(byte which, CSequenceStep step_for_transpose, byte midi_vel_accent, byte midi_vel, byte action_gate) {


		// determine the step value to be processed, which may come from a different layer
		// in the case of transposition
		CSequenceStep step;
		step.reset_all();
		int transposed;
		switch(m_cfg.m_mode) {
		case V_SQL_SEQ_MODE_TRANSPOSE_ALL:
			transposed = (int)step_for_transpose.m_value + m_state.m_step_value.m_value - OFFSET_ZERO;
			if(transposed >= 0 && transposed < 128) {
				step = step_for_transpose;
				step.m_value = transposed;
			}
			break;
		case V_SQL_SEQ_MODE_TRANSPOSE_LOCK:
			 if(step_for_transpose.m_is_accent) {
				 transposed = (int)step_for_transpose.m_value + m_state.m_step_value.m_value - OFFSET_ZERO;
				 if(transposed >= 0 && transposed < 128) {
					step = step_for_transpose;
					step.m_value = transposed;
				 }
			 }
			 else {
				step = step_for_transpose;
				step.m_is_accent = 0;
			 }
			 break;
		default:
			step = m_state.m_step_value;
		}


		// Get step type: active / legato / velocity level
		int glide_time = 0;
		CCVGate::GATE_STATE gate_state = CCVGate::GATE_CLOSED;
		byte legato = 0;
		byte velocity = 0;
		byte active = step.m_is_data_point;
		if(step.m_is_accent) {
			velocity = midi_vel_accent;
		}
		else if(step.m_is_trigger) {
			velocity = midi_vel;
		}
		else {
			// active step with no trigger or no gate
			legato = 1;
			if(m_cfg.m_cv_glide == V_SQL_CVGLIDE_ON) {
				glide_time = g_clock.get_ms_for_measure(m_cfg.m_step_rate);
			}
		}

		// Kill "open" notes which are timed to step boundaries rather than by a timeout
		byte kill_open_notes = 1;
		if(m_cfg.m_note_dur == V_SQL_STEP_DUR_STEP) {
			kill_open_notes = !legato;
		}
		else if(m_cfg.m_note_dur == V_SQL_STEP_DUR_FULL) {
			kill_open_notes = active && !legato;
		}
		if(kill_open_notes) {
			for(int i=0; i<MAX_PLAYING_NOTES;++i) {
				if(m_state.m_playing[i].note && !m_state.m_playing[i].count) {
					if(m_state.m_playing[i].note == m_state.m_last_note) {
						gate_state = CCVGate::GATE_CLOSED;
					}
					send_midi_note(m_state.m_playing[i].note,0);
					m_state.m_playing[i].note = 0;
				}
			}
		}


		// is there anything going on at this step
		if(active) {

			// force to scale if needed
			byte note = step.m_value;
			if(m_cfg.m_mode == V_SQL_SEQ_MODE_SCALE) {
				note = g_scale.index_to_note(note);
			}
			else if(m_cfg.m_force_scale == V_SQL_FORCE_SCALE_ON) {
				note = g_scale.force_to_scale(note);
			}

			if(note) {
				// decide duration. a duration 0 is open ended
				byte duration = 0;
				if(m_cfg.m_note_dur >= V_SQL_STEP_DUR_32) {
					duration = c_step_duration[m_cfg.m_note_dur - V_SQL_STEP_DUR_32];
				}

				// scan list of playing note slots to find a usable slot where
				// we can track this note
				int free = -1;
				int same = -1;
				int last = -1;
				int steal = -1;
				int steal_count = -1;
				for(int i=0; i<MAX_PLAYING_NOTES;++i) {
					if(!m_state.m_playing[i].note) {
						// we have a free slot
						free = i;
					}
					else if(m_state.m_playing[i].note == note) {
						// the same note is already playing. we will use this slot so
						// there is no need to carry on looking
						same = i;
						break;
					}
					else if(m_state.m_playing[i].note == m_state.m_last_note) {
						// this slot is playing the last note output from this channel
						// (and it is still playing!)
						last = i;
					}
					else if(m_state.m_playing[i].count < steal_count || steal_count == -1) {
						// this is the note that has the least time remaining to play
						// so we might steal it's slot if we have to...
						steal_count = m_state.m_playing[i].count;
						steal = i;
					}
				}

				// For legato steps there is no velocity information... we use the
				// velocity for the last played note
				if(velocity) {
					m_state.m_last_velocity = velocity;
				}

				// decide which of the possible slots we're going to use
				int slot = -1;
				if(same>=0) {
					// slot for same note will always be reused
					if(!legato) {
						// retrigger the note
						send_midi_note(note,0);
						send_midi_note(note,m_state.m_last_velocity);
						gate_state = CCVGate::GATE_RETRIG;
					}
					slot = same;
				}
				else if(last>=0 && legato) {
					// the last played note is still sounding, in legato mode we will
					// replace that note, but only stop it after the new note starts
					send_midi_note(note,m_state.m_last_velocity);
					send_midi_note(m_state.m_playing[last].note,0);
					gate_state = CCVGate::GATE_OPEN;
					slot = last;
				}
				else if(free>=0) {
					// else a free slot will be used if available
					send_midi_note(note,m_state.m_last_velocity);
					gate_state = legato? CCVGate::GATE_OPEN : CCVGate::GATE_RETRIG;
					slot = free;
				}
				else if(steal>=0) {
					// final option we'll steal a slot from another note, so we need to stop that note playing
					send_midi_note(m_state.m_playing[steal].note,0);
					send_midi_note(note,m_state.m_last_velocity);
					gate_state = legato? CCVGate::GATE_OPEN : CCVGate::GATE_RETRIG;
					slot = steal;
				}

				if(slot >= 0) {
					m_state.m_last_note = note;
					m_state.m_playing[slot].note = note;
					m_state.m_playing[slot].count = duration;
					g_cv_gate.pitch_cv(which, note, m_cfg.m_cv_scale, glide_time);
					if(action_gate) {
						g_cv_gate.gate(which, gate_state);
					}
				}
			}
		}
	}
*/


	///////////////////////////////////////////////////////////////////////////////
	// Play a step for a note mode
//TODO - MIDI
	void action_step_pitch(byte which) {


		// get the note we need to play, taking into account being
		// forced into a scale
		byte note = m_state.m_step_value.get_value();
		if(m_cfg.m_force_scale == V_SQL_FORCE_SCALE_ON) {
			note = g_scale.force_to_scale(note);
		}
		// set the note pitch
		g_cv_gate.pitch_cv(which, note, m_cfg.m_cv_scale, 0);

	}

#if 0
	///////////////////////////////////////////////////////////////////////////////
	// Play a step for a note mode
//TODO - MIDI
	void action_step_note(byte which, CSequenceStep step_for_transpose, byte action_gate) {


		CSequenceStep step;
		if(m_cfg.m_mode == V_SQL_SEQ_MODE_TRANSPOSE) { // IS THIS A TRANSPOSE LAYER?

			// the step to be transposed comes from a different layer and is passed
			// in as a parameter
			step = step_for_transpose;

			// allow transposition to be "locked out" by selected steps
			byte do_transpose = 1;
			if(m_cfg.m_tran_acc == V_SQL_TRAN_ACC_LOCK) {
/*				if(step.is_accent()) {
					// this this mode the accent flag is used to mean that this
					// step should not be transposed. It should not be treated
					// as an accent
					do_transpose = 0;
					step.clear_accent();
				}*/
			}

			// actually perform the transposition if still needed. Do not transpose values that
			// would be out of range of MIDI notes
			if(do_transpose) {
				int transposed = (int)step_for_transpose.get_value() + (int)m_state.m_step_value.get_value() - OFFSET_ZERO;
				if(transposed >= 0 && transposed <= 127) {
					step.set_value(transposed);
				}
			}

			/*
			// set the trigger information on the step
			switch(m_cfg.m_tran_trig) {
			case V_SQL_TRAN_TRIG_AND:
				step.set_gate(step_for_transpose.get_gate() & m_state.m_step_value.get_gate());
				break;
			case V_SQL_TRAN_TRIG_OR:
				step.set_gate(step_for_transpose.get_gate() | m_state.m_step_value.get_gate());
				break;
			case V_SQL_TRAN_TRIG_XOR:
				step.set_gate(step_for_transpose.get_gate() ^ m_state.m_step_value.get_gate());
				break;
			case V_SQL_TRAN_TRIG_ORIG:
				step.set_gate(step_for_transpose.get_gate());
				break;
			case V_SQL_TRAN_TRIG_THIS:
			default:
				step.set_gate(m_state.m_step_value.get_gate());
				break;
			}*/

			step.copy_gate(m_state.m_step_value);

		}
		else {

			// not a transpose layer, simply store the current step
			step = m_state.m_step_value;
		}

		// the CV will only be applied if there is an open gate
		if(step.is_gate()) {

			// get the note we need to play, taking into account being
			// forced into a scale
			byte note = step.get_value();
			if(m_cfg.m_force_scale == V_SQL_FORCE_SCALE_ON) {
				note = g_scale.force_to_scale(note);
			}

			// is there a trigger at this step?
			if(step.is_gate()) {

				//CSequenceStep next_step;

				// set the note pitch
				g_cv_gate.pitch_cv(which, note, m_cfg.m_cv_scale, 0);

				// trigger the gate
				g_cv_gate.gate(which, CCVGate::GATE_RETRIG);

				// stop the previous MIDI note before new note (if not legato mode)
				if(m_state.m_midi_note && m_cfg.m_note_dur != V_SQL_NOTE_DUR_LEGA) {
					send_midi_note(m_state.m_midi_note, 0);
				}
				// send the new one
				if(note) {
					send_midi_note(note, m_cfg.m_midi_vel);
					//send_midi_note(note, step.is_accent() ? m_cfg.m_midi_vel_accent : m_cfg.m_midi_vel);
				}
				// stop the previous MIDI note after new note (if legato mode)
				if(m_state.m_midi_note && m_cfg.m_note_dur == V_SQL_NOTE_DUR_LEGA) {
					send_midi_note(m_state.m_midi_note, 0);
				}
				m_state.m_midi_note = note;

				// set the appropriate note duration
				switch(m_cfg.m_note_dur) {
				case V_SQL_NOTE_DUR_OPEN:
				case V_SQL_NOTE_DUR_LEGA:
					m_state.m_gate_timeout = INFINITE_GATE;	// stay open until the next note
					break;
				case V_SQL_NOTE_DUR_TRIG:
					m_state.m_gate_timeout = CCVGate::TRIG_DURATION; // just a short trigger pulse
					break;
				case V_SQL_NOTE_DUR_100:
					m_state.m_gate_timeout = 0; // until the next step
					break;
				default: // other enumerations have integer values 0-10
					// check whether the next step is open gate without a trigger,
					//next_step = m_cfg.m_step[next_step_index(m_state.m_play_pos)];
					//if(next_step.is_gate() && !next_step.is_trigger()) {
					if(step.is_tied()) {
						// this will extend the current note to a full step
						m_state.m_gate_timeout = 0;
					}
					else {
						// otherwise use the set duration
 						m_state.m_gate_timeout = (g_clock.get_ms_per_measure(m_cfg.m_step_rate) * m_cfg.m_note_dur ) / 10;
					}
					break;
				}

			}
			else
			{

				// send the new one
				if(note) {
//					send_midi_note(note, step.is_accent() ? m_cfg.m_midi_vel_accent : m_cfg.m_midi_vel);
					send_midi_note(note, m_cfg.m_midi_vel);
				}
				// stop the previous MIDI note after new note
				if(m_state.m_midi_note) {
					send_midi_note(m_state.m_midi_note, 0);
				}
				m_state.m_midi_note = note;

				// check if we need to glide to the note
				if(m_cfg.m_cv_glide == V_SQL_CVGLIDE_ON) {
					int glide_time = g_clock.get_ms_per_measure(m_cfg.m_step_rate);
					g_cv_gate.pitch_cv(which, note, m_cfg.m_cv_scale, glide_time);
				}
				else {
					// no glide, set pitch directly
					g_cv_gate.pitch_cv(which, note, m_cfg.m_cv_scale, 0);
				}

				// open the gate for one step
				g_cv_gate.gate(which, CCVGate::GATE_OPEN);
				m_state.m_gate_timeout = 0;
			}
		}
		else {
			if(!m_state.m_gate_timeout) {
				// close the gate
				g_cv_gate.gate(which, CCVGate::GATE_CLOSED);
				if(m_state.m_midi_note) {
					send_midi_note(m_state.m_midi_note, 0);
					m_state.m_midi_note = 0;
				}
			}
		}

	}
#endif

	///////////////////////////////////////////////////////////////////////////////
	void action_step_mod(byte which) {
		CSequencePage& page = get_page(m_state.m_play_page_no);

		byte value1 = page.get_step(m_state.m_play_pos).get_value();
		if(m_cfg.m_cv_glide == V_SQL_CVGLIDE_ON) {

			int page_no = m_state.m_play_page_no;
			int step_no = m_state.m_play_page_no;
			int page_list_pos = m_state.m_page_list_pos;
			byte page_advanced;
			calc_next_step(page_no, step_no, page_advanced, page_list_pos);

			byte value2 = get_step(page_no, step_no).get_value();
			int ms = g_clock.get_ms_per_measure(m_cfg.m_step_rate);
			g_cv_gate.mod_cv(which, value1, m_cfg.m_cv_range, value2, ms);
		}
		else {
			g_cv_gate.mod_cv(which, value1, m_cfg.m_cv_range, 0, 0);
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	// Play the gate for a step
	void action_step_gate(byte which) {
		if(m_state.m_step_value.is_gate()) {
			// set the appropriate note duration
			switch(m_cfg.m_note_dur) {
			case V_SQL_NOTE_DUR_OPEN:
			case V_SQL_NOTE_DUR_LEGA:
				m_state.m_gate_timeout = INFINITE_GATE;	// stay open until the next gate
				break;
			case V_SQL_NOTE_DUR_TRIG:
				m_state.m_gate_timeout = CCVGate::TRIG_DURATION; // just a short trigger pulse
				break;
			case V_SQL_NOTE_DUR_100:
				m_state.m_gate_timeout = 0; // until the next step
				break;
			default: // other enumerations have integer values 0-10
				m_state.m_gate_timeout = (g_clock.get_ms_per_measure(m_cfg.m_step_rate) * m_cfg.m_note_dur ) / 10;
				break;
			}
		}
		else {
			if(!m_state.m_gate_timeout) {
				g_cv_gate.gate(which, CCVGate::GATE_CLOSED);
			}
		}
	}

/*		if(step.is_gate()) {
			g_cv_gate.gate(which, CCVGate::GATE_RETRIG);
		}
		else if(step.is_tied()) {
			g_cv_gate.gate(which, CCVGate::GATE_OPEN);
		}
		else {
			g_cv_gate.gate(which, CCVGate::GATE_CLOSED);
		}
	}*/



};

//TODO
//const byte CSequenceLayer::c_tick_rates[V_SQL_STEP_RATE_MAX] = {96,72,48,36,32,24,18,16,12,9,8,6,4,3};
//const byte CSequenceLayer::c_step_duration[] = {3,4,6,8,9,12,16,18,24,32,36,48,72,96};

#endif /* SEQUENCE_LAYER_H_ */
