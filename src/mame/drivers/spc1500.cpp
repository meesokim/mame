// license:BSD-3-Clause
// copyright-holders:Miso Kim
/***************************************************************************

Samsung SPC-1500 driver by Miso Kim

    2015-12-16 Preliminary driver.
ToDo:

****************************************************************************/

#include "emu.h"
#include "cpu/z80/z80.h"
#include "machine/ram.h"
#include "machine/i8255.h"
#include "sound/ay8910.h"
#include "sound/wave.h"
#include "video/mc6845.h"
#include "imagedev/cassette.h"
#include "formats/spc1000_cas.h"
#include "bus/centronics/ctronics.h"
#define VDP_CLOCK  XTAL_42_9545MHz
#include "softlist.h"

struct scrn_reg_t
{
	UINT8 gfx_bank;
	UINT8 disp_bank;
	UINT8 pcg_mode;
	UINT8 v400_mode;
	UINT8 ank_sel;

	UINT8 pri;
	UINT8 blackclip; // x1 turbo specific
};

class spc1500_state : public driver_device
{
public:
	spc1500_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_vdg(*this, "mc6845")
		, m_cass(*this, "cassette")
		, m_ram(*this, RAM_TAG)
		, m_p_videoram(*this, "videoram")
		, m_io_kb(*this, "LINE")
		, m_io_joy(*this, "JOY")
		, m_centronics(*this, "centronics")
		, m_pio(*this, "8255")
		, m_palette(*this, "palette")
	{}

	DECLARE_WRITE8_MEMBER(bank0_rom);
	DECLARE_WRITE8_MEMBER(bank0_ram);
	DECLARE_WRITE_LINE_MEMBER(irq_w);
	DECLARE_READ8_MEMBER(porta_r);
	DECLARE_WRITE_LINE_MEMBER( centronics_busy_w ) { m_centronics_busy = state; }
	DECLARE_READ8_MEMBER(mc6845_videoram_r);
	DECLARE_WRITE8_MEMBER(cass_w);
	DECLARE_READ8_MEMBER(keyboard_r);
	DECLARE_WRITE8_MEMBER(paletb_w);
	DECLARE_WRITE8_MEMBER(paletr_w);
	DECLARE_WRITE8_MEMBER(paletg_w);
	DECLARE_WRITE8_MEMBER(priority_w);
	DECLARE_WRITE8_MEMBER(pcgg_w);
	DECLARE_WRITE8_MEMBER(pcgb_w);
	DECLARE_WRITE8_MEMBER(pcgr_w);
	DECLARE_READ8_MEMBER(pcgg_r);
	DECLARE_READ8_MEMBER(pcgb_r);
	DECLARE_READ8_MEMBER(pcgr_r);
	DECLARE_WRITE8_MEMBER(crtc_w);
	DECLARE_READ8_MEMBER(crtc_r);
	DECLARE_WRITE8_MEMBER(romsel);
	DECLARE_WRITE8_MEMBER(ramsel);
	DECLARE_WRITE8_MEMBER(porta_w);
	DECLARE_WRITE8_MEMBER(portb_w);
	DECLARE_WRITE8_MEMBER(portc_w);
	DECLARE_READ8_MEMBER(portb_r);
	DECLARE_PALETTE_INIT(spc);
	DECLARE_VIDEO_START(spc);
	UINT32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	void draw_pixel(bitmap_rgb32 &bitmap,int y,int x,UINT16 pen,UINT8 width,UINT8 height);
	UINT8 check_prev_height(int x,int y,int x_size);
	UINT8 check_line_valid_height(int y,int x_size,int height);	
	void draw_fgtilemap(bitmap_rgb32 &bitmap,const rectangle &cliprect);
	void draw_gfxbitmap(bitmap_rgb32 &bitmap,const rectangle &cliprect, int plane,int pri);	
	int priority_mixer_pri(int color);
private:
	UINT8 m_ipl;
	UINT8 m_GMODE;
	UINT16 m_page;
	UINT8 *m_work_ram;
	attotime m_time;
	UINT8 m_crtc_vreg[0x100];
	bool m_centronics_busy;
	virtual void machine_start() override;
	virtual void machine_reset() override;
	required_device<z80_device> m_maincpu;
	required_device<mc6845_device> m_vdg;
	required_device<cassette_image_device> m_cass;
	required_device<ram_device> m_ram;
	required_shared_ptr<UINT8> m_p_videoram;
	required_ioport_array<10> m_io_kb;
	required_ioport m_io_joy;
	required_device<centronics_device> m_centronics;
	required_device<i8255_device> m_pio;
	required_device<palette_device> m_palette;	
	std::unique_ptr<UINT8[]> m_tvram;         /**< Pointer for Text Video RAM */
	std::unique_ptr<UINT8[]> m_avram;         /**< Pointer for Attribute Video RAM */
	std::unique_ptr<UINT8[]> m_kvram;         /**< Pointer for Extended Kanji Video RAM (X1 Turbo) */	
	std::unique_ptr<UINT8[]> m_gfx_bitmap_ram;    /**< Pointer for bitmap layer RAM. */
	std::unique_ptr<UINT8[]> m_pcg_ram;       /**< Pointer for PCG GFX RAM */		
	UINT8 *m_cg_rom;        /**< Pointer for GFX ROM */
	UINT8 m_is_turbo;       /**< Machine type: (0) X1 Vanilla, (1) X1 Turbo */
	int m_xstart,           /**< Start X offset for screen drawing. */
		m_ystart;           /**< Start Y offset for screen drawing. */
	UINT8 *m_Hangul_rom;     /**< Pointer for Kanji ROMs */		
	scrn_reg_t m_scrn_reg;      /**< Base Video Registers. */	
};

#define mc6845_h_char_total     (m_crtc_vreg[0])
#define mc6845_h_display        (m_crtc_vreg[1])
#define mc6845_h_sync_pos       (m_crtc_vreg[2])
#define mc6845_sync_width       (m_crtc_vreg[3])
#define mc6845_v_char_total     (m_crtc_vreg[4])
#define mc6845_v_total_adj      (m_crtc_vreg[5])
#define mc6845_v_display        (m_crtc_vreg[6])
#define mc6845_v_sync_pos       (m_crtc_vreg[7])
#define mc6845_mode_ctrl        (m_crtc_vreg[8])
#define mc6845_tile_height      (m_crtc_vreg[9]+1)
#define mc6845_cursor_y_start   (m_crtc_vreg[0x0a])
#define mc6845_cursor_y_end     (m_crtc_vreg[0x0b])
#define mc6845_start_addr       (((m_crtc_vreg[0x0c]<<8) & 0xff00) | (m_crtc_vreg[0x0d] & 0xff))
#define mc6845_cursor_addr      (((m_crtc_vreg[0x0e]<<8) & 0xff00) | (m_crtc_vreg[0x0f] & 0xff))
#define mc6845_light_pen_addr   (((m_crtc_vreg[0x10]<<8) & 0xff00) | (m_crtc_vreg[0x11] & 0xff))
#define mc6845_update_addr      (((m_crtc_vreg[0x12]<<8) & 0xff00) | (m_crtc_vreg[0x13] & 0xff))

static ADDRESS_MAP_START(spc1500_mem, AS_PROGRAM, 8, spc1500_state )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x7fff) AM_READ_BANK("bank1") AM_WRITE_BANK("bank2")
	AM_RANGE(0x8000, 0xffff) AM_READ_BANK("bank3") AM_WRITE_BANK("bank4")
ADDRESS_MAP_END

WRITE8_MEMBER( spc1500_state::cass_w )
{
	attotime time = machine().scheduler().time();
	m_cass->output(BIT(data, 0) ? -1.0 : 1.0);
	if (BIT(data, 1) && ATTOSECONDS_IN_MSEC((time - m_time).as_attoseconds()) > 500) {
		m_cass->change_state((m_cass->get_state() & CASSETTE_MASK_MOTOR) == CASSETTE_MOTOR_DISABLED ? CASSETTE_MOTOR_ENABLED : CASSETTE_MOTOR_DISABLED, CASSETTE_MASK_MOTOR);
		m_time = time;
	}
	m_centronics->write_strobe(BIT(data, 2) ? true : false);
}

READ8_MEMBER( spc1500_state::keyboard_r )
{
	// most games just read kb in $8000-$8009 but a few of them
	// (e.g. Toiler Adventure II and Vela) use mirrored addr instead
	offset &= 0xf;

	if (offset <= 9)
		return m_io_kb[offset]->read();
	else
		return 0xff;
}

WRITE8_MEMBER( spc1500_state::romsel)
{
	if (m_ipl)
		membank("bank1")->set_entry(1);
	else
		membank("bank1")->set_entry(2);		
}

WRITE8_MEMBER( spc1500_state::ramsel)
{
	membank("bank1")->set_entry(0);
}

WRITE8_MEMBER( spc1500_state::porta_w)
{
	
}

WRITE8_MEMBER( spc1500_state::portb_w)
{
	m_ipl = data & 1 << 1;
}

WRITE8_MEMBER( spc1500_state::portc_w)
{
	
}

READ8_MEMBER( spc1500_state::portb_r)
{
	return 0;
}

WRITE8_MEMBER( spc1500_state::crtc_w)
{
	
}

READ8_MEMBER( spc1500_state::crtc_r)
{
	return 0;
}

WRITE8_MEMBER( spc1500_state::pcgg_w)
{
	
}

READ8_MEMBER( spc1500_state::pcgg_r)
{
	return 0;
}

WRITE8_MEMBER( spc1500_state::pcgr_w)
{
	
}

READ8_MEMBER( spc1500_state::pcgr_r)
{
	return 0;
}

WRITE8_MEMBER( spc1500_state::pcgb_w)
{
	
}

READ8_MEMBER( spc1500_state::pcgb_r)
{
	return 0;
}

WRITE8_MEMBER( spc1500_state::priority_w)
{
	
}

WRITE8_MEMBER( spc1500_state::paletg_w)
{
	
}

WRITE8_MEMBER( spc1500_state::paletb_w)
{
	
}

WRITE8_MEMBER( spc1500_state::paletr_w)
{
	
}


/*************************************
 *
 *  Video Functions
 *
 *************************************/

VIDEO_START_MEMBER(spc1500_state, spc)
{
	m_avram = make_unique_clear<UINT8[]>(0x800);
	m_tvram = make_unique_clear<UINT8[]>(0x800);
	m_kvram = make_unique_clear<UINT8[]>(0x800);
	m_gfx_bitmap_ram = make_unique_clear<UINT8[]>(0xc000*2);
}

void spc1500_state::draw_pixel(bitmap_rgb32 &bitmap,int y,int x,UINT16 pen,UINT8 width,UINT8 height)
{
	if(!machine().first_screen()->visible_area().contains(x, y))
		return;

	if(width && height)
	{
		bitmap.pix32(y+0+m_ystart, x+0+m_xstart) = m_palette->pen(pen);
		bitmap.pix32(y+0+m_ystart, x+1+m_xstart) = m_palette->pen(pen);
		bitmap.pix32(y+1+m_ystart, x+0+m_xstart) = m_palette->pen(pen);
		bitmap.pix32(y+1+m_ystart, x+1+m_xstart) = m_palette->pen(pen);
	}
	else if(width)
	{
		bitmap.pix32(y+m_ystart, x+0+m_xstart) = m_palette->pen(pen);
		bitmap.pix32(y+m_ystart, x+1+m_xstart) = m_palette->pen(pen);
	}
	else if(height)
	{
		bitmap.pix32(y+0+m_ystart, x+m_xstart) = m_palette->pen(pen);
		bitmap.pix32(y+1+m_ystart, x+m_xstart) = m_palette->pen(pen);
	}
	else
		bitmap.pix32(y+m_ystart, x+m_xstart) = m_palette->pen(pen);
}

/* adjust tile index when we are under double height condition */
UINT8 spc1500_state::check_prev_height(int x,int y,int x_size)
{
	UINT8 prev_tile = m_tvram[(x+((y-1)*x_size)+mc6845_start_addr) & 0x7ff];
	UINT8 cur_tile = m_tvram[(x+(y*x_size)+mc6845_start_addr) & 0x7ff];
	UINT8 prev_attr = m_avram[(x+((y-1)*x_size)+mc6845_start_addr) & 0x7ff];
	UINT8 cur_attr = m_avram[(x+(y*x_size)+mc6845_start_addr) & 0x7ff];

	if(prev_tile == cur_tile && prev_attr == cur_attr)
		return 8;

	return 0;
}

UINT8 spc1500_state::check_line_valid_height(int y,int x_size,int height)
{
	UINT8 line_attr = m_avram[(0+(y*x_size)+mc6845_start_addr) & 0x7ff];

	if((line_attr & 0x40) == 0)
		return 0;

	return height;
}

void spc1500_state::draw_fgtilemap(bitmap_rgb32 &bitmap,const rectangle &cliprect)
{
	/*
	    attribute table:
	    x--- ---- double width
	    -x-- ---- double height
	    --x- ---- PCG select
	    ---x ---- color blinking
	    ---- x--- reverse color
	    ---- -xxx color pen

	    x--- ---- select Hangul ROM
	    -x-- ---- Hangul side (0=left, 1=right)
	    --x- ---- Underline
	    ---x ---- Hangul ROM select (0=level 1, 1=level 2) (TODO: implement this)
	    ---- xxxx Hangul upper 4 bits
	*/

	int y,x,res_x,res_y;
	UINT32 tile_offset;
	UINT8 x_size,y_size;

	x_size = mc6845_h_display;
	y_size = mc6845_v_display;

	if(x_size == 0 || y_size == 0)
		return; //don't bother if screen is off

	if(x_size != 80 && x_size != 40 && y_size != 25)
		popmessage("%d %d",x_size,y_size);

	for (y=0;y<y_size;y++)
	{
		for (x=0;x<x_size;x++)
		{
			int tile = m_tvram[((x+y*x_size)+mc6845_start_addr) & 0x7ff];
			int color = m_avram[((x+y*x_size)+mc6845_start_addr) & 0x7ff] & 0x1f;
			int width = BIT(m_avram[((x+y*x_size)+mc6845_start_addr) & 0x7ff], 7);
			int height = BIT(m_avram[((x+y*x_size)+mc6845_start_addr) & 0x7ff], 6);
			int pcg_bank = BIT(m_avram[((x+y*x_size)+mc6845_start_addr) & 0x7ff], 5);
			UINT8 *gfx_data = pcg_bank ? m_pcg_ram.get() : m_cg_rom; //machine.root_device().memregion(pcg_bank ? "pcg" : "cgrom")->base();
			int knj_enable = 0;
			//int knj_side = 0;
			//int knj_bank = 0;
			int knj_uline = 0;
			{
				int pen[3],pen_mask,pcg_pen,xi,yi,dy;

				pen_mask = color & 7;

				dy = 0;

				height = check_line_valid_height(y,x_size,height);

				if(height && y)
					dy = check_prev_height(x,y,x_size);

				/* guess: assume that Hangul VRAM doesn't double the vertical size */
				if(knj_enable) { height = 0; }

				for(yi=0;yi<mc6845_tile_height;yi++)
				{
					for(xi=0;xi<8;xi++)
					{
						if(knj_enable) //Hangul select
						{
							tile_offset  = tile * 16;
							tile_offset += (yi+dy*(m_scrn_reg.v400_mode+1)) >> (height+m_scrn_reg.v400_mode);
							pen[0] = gfx_data[tile_offset+0x0000]>>(7-xi) & (pen_mask & 1)>>0;
							pen[1] = gfx_data[tile_offset+0x0000]>>(7-xi) & (pen_mask & 2)>>1;
							pen[2] = gfx_data[tile_offset+0x0000]>>(7-xi) & (pen_mask & 4)>>2;

							if(yi == mc6845_tile_height-1 && knj_uline) //underlined attribute
							{
								pen[0] = (pen_mask & 1)>>0;
								pen[1] = (pen_mask & 2)>>1;
								pen[2] = (pen_mask & 4)>>2;
							}

							if((yi >= 16 && m_scrn_reg.v400_mode == 0) || (yi >= 32 && m_scrn_reg.v400_mode == 1))
								pen[0] = pen[1] = pen[2] = 0;
						}
						else if(pcg_bank) // PCG
						{
							tile_offset  = tile * 8;
							tile_offset += (yi+dy*(m_scrn_reg.v400_mode+1)) >> (height+m_scrn_reg.v400_mode);

							pen[0] = gfx_data[tile_offset+0x0000]>>(7-xi) & (pen_mask & 1)>>0;
							pen[1] = gfx_data[tile_offset+0x0800]>>(7-xi) & (pen_mask & 2)>>1;
							pen[2] = gfx_data[tile_offset+0x1000]>>(7-xi) & (pen_mask & 4)>>2;

							if((yi >= 8 && m_scrn_reg.v400_mode == 0) || (yi >= 16 && m_scrn_reg.v400_mode == 1))
								pen[0] = pen[1] = pen[2] = 0;
						}
						else
						{
							tile_offset  = tile * (8*(m_scrn_reg.ank_sel+1));
							tile_offset += (yi+dy*(m_scrn_reg.v400_mode+1)) >> (height+m_scrn_reg.v400_mode);

							pen[0] = gfx_data[tile_offset+m_scrn_reg.ank_sel*0x0800]>>(7-xi) & (pen_mask & 1)>>0;
							pen[1] = gfx_data[tile_offset+m_scrn_reg.ank_sel*0x0800]>>(7-xi) & (pen_mask & 2)>>1;
							pen[2] = gfx_data[tile_offset+m_scrn_reg.ank_sel*0x0800]>>(7-xi) & (pen_mask & 4)>>2;

							if(m_scrn_reg.ank_sel)
							{
								if((yi >= 16 && m_scrn_reg.v400_mode == 0) || (yi >= 32 && m_scrn_reg.v400_mode == 1))
									pen[0] = pen[1] = pen[2] = 0;
							}
							else
							{
								if((yi >=  8 && m_scrn_reg.v400_mode == 0) || (yi >= 16 && m_scrn_reg.v400_mode == 1))
									pen[0] = pen[1] = pen[2] = 0;
							}
						}

						pcg_pen = pen[2]<<2|pen[1]<<1|pen[0]<<0;

						if(color & 0x10 && machine().first_screen()->frame_number() & 0x10) //reverse flickering
							pcg_pen^=7;

						if(pcg_pen == 0 && (!(color & 8)))
							continue;

						if(color & 8) //revert the used color pen
							pcg_pen^=7;

						if((m_scrn_reg.blackclip & 8) && (color == (m_scrn_reg.blackclip & 7)))
							pcg_pen = 0; // clip the pen to black

						res_x = x*8+xi*(width+1);
						res_y = y*(mc6845_tile_height)+yi;

						if(res_y < cliprect.min_y || res_y > cliprect.max_y) // partial update, TODO: optimize
							continue;

						draw_pixel(bitmap,res_y,res_x,pcg_pen,width,0);
					}
				}
			}

			if(width) //skip next char if we are under double width condition
				x++;
		}
	}
}

/*
 * Priority Mixer Calculation (pri)
 *
 * If pri is 0xff then the bitmap entirely covers the tilemap, if it's 0x00 then
 * the tilemap priority is entirely above the bitmap. Any other value mixes the
 * bitmap and the tilemap priorities based on the pen value, bit 0 = entry 0 <-> bit 7 = entry 7
 * of the bitmap.
 *
 */
int spc1500_state::priority_mixer_pri(int color)
{
	int pri_i,pri_mask_calc;

	pri_i = 0;
	pri_mask_calc = 1;

	while(pri_i < 7)
	{
		if((color & 7) == pri_i)
			break;

		pri_i++;
		pri_mask_calc<<=1;
	}

	return pri_mask_calc;
}

void spc1500_state::draw_gfxbitmap(bitmap_rgb32 &bitmap,const rectangle &cliprect, int plane,int pri)
{
	int xi,yi,x,y;
	int pen_r,pen_g,pen_b,color;
	int pri_mask_val;
	UINT8 x_size,y_size;
	int gfx_offset;

	x_size = mc6845_h_display;
	y_size = mc6845_v_display;

	if(x_size == 0 || y_size == 0)
		return; //don't bother if screen is off

	if(x_size != 80 && x_size != 40 && y_size != 25)
		popmessage("%d %d",x_size,y_size);

	//popmessage("%04x %02x",mc6845_start_addr,mc6845_tile_height);

	for (y=0;y<y_size;y++)
	{
		for(x=0;x<x_size;x++)
		{
			for(yi=0;yi<(mc6845_tile_height);yi++)
			{
				for(xi=0;xi<8;xi++)
				{
					gfx_offset = ((x+(y*x_size)) + mc6845_start_addr) & 0x7ff;
					gfx_offset+= ((yi >> m_scrn_reg.v400_mode) * 0x800) & 0x3fff;
					pen_b = (m_gfx_bitmap_ram[gfx_offset+0x0000+plane*0xc000]>>(7-xi)) & 1;
					pen_r = (m_gfx_bitmap_ram[gfx_offset+0x4000+plane*0xc000]>>(7-xi)) & 1;
					pen_g = (m_gfx_bitmap_ram[gfx_offset+0x8000+plane*0xc000]>>(7-xi)) & 1;

					color =  (pen_g<<2 | pen_r<<1 | pen_b<<0) | 8;

					pri_mask_val = priority_mixer_pri(color);
					if(pri_mask_val & pri) continue;

					if((color == 8 && m_scrn_reg.blackclip & 0x10) || (color == 9 && m_scrn_reg.blackclip & 0x20)) // bitmap color clip to black conditions
						color = 0;

					if(y*(mc6845_tile_height)+yi < cliprect.min_y || y*(mc6845_tile_height)+yi > cliprect.max_y) // partial update TODO: optimize
						continue;

					draw_pixel(bitmap,y*(mc6845_tile_height)+yi,x*8+xi,color,0,0);
				}
			}
		}
	}
}

UINT32 spc1500_state::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	bitmap.fill(rgb_t(0xff,0x00,0x00,0x00), cliprect);

	/* TODO: correct calculation thru mc6845 regs */
	m_xstart = ((mc6845_h_char_total - mc6845_h_sync_pos) * 8) / 2;
	m_ystart = ((mc6845_v_char_total - mc6845_v_sync_pos) * 8) / 2;

//  popmessage("%d %d %d %d",mc6845_h_sync_pos,mc6845_v_sync_pos,mc6845_h_char_total,mc6845_v_char_total);

	draw_gfxbitmap(bitmap,cliprect,m_scrn_reg.disp_bank,m_scrn_reg.pri);
	draw_fgtilemap(bitmap,cliprect);
	draw_gfxbitmap(bitmap,cliprect,m_scrn_reg.disp_bank,m_scrn_reg.pri^0xff);

	return 0;
}


static ADDRESS_MAP_START( spc1500_io , AS_IO, 8, spc1500_state )
	ADDRESS_MAP_UNMAP_HIGH
//	AM_RANGE(0x0000, 0x03ff) AM_DEVREADWRITE("userio", user_device, userio_r, userio_w)
//	AM_RANGE(0x0400, 0x05ff) AM_DEVREADWRITE("lanio", lan_device, lanio_r, lanio_w)
//	AM_RANGE(0x0600, 0x07ff) AM_DEVREADWRITE("rs232c", rs232c_device, rs232c_r, rs232c_w)
//	AM_RANGE(0x0800, 0x09ff) AM_DEVREADWRITE("fdcx", fdcx_device, fdcx_r, fdcx_w)
//	AM_RANGE(0x0a00, 0x0bff) AM_DEVREADWRITE("userio", user_device, userio_r, userio_w)
//	AM_RANGE(0x0c00, 0x0dff) AM_DEVREADWRITE("fdc", fdc_device, fdc_r, fdc_w)
//	AM_RANGE(0x0e00, 0x0fff) AM_DEVREADWRITE("extram", extram_device, extram_r, extram_w)
	AM_RANGE(0x1000, 0x10ff) AM_DEVWRITE("paletB", spc1500_state, paletb_w)
	AM_RANGE(0x1100, 0x11ff) AM_DEVWRITE("paletR", spc1500_state, paletr_w)
	AM_RANGE(0x1200, 0x12ff) AM_DEVWRITE("paletG", spc1500_state, paletg_w)
	AM_RANGE(0x1300, 0x13ff) AM_DEVWRITE("priority", spc1500_state, priority_w)
	AM_RANGE(0x1400, 0x14ff) AM_DEVREAD("pcggin", spc1500_state, pcgg_r)
	AM_RANGE(0x1500, 0x15ff) AM_DEVREADWRITE("pcgbrw", spc1500_state, pcgb_r, pcgb_w)
	AM_RANGE(0x1600, 0x16ff) AM_DEVREADWRITE("pcgrrw", spc1500_state, pcgr_r, pcgr_w)
	AM_RANGE(0x1700, 0x17ff) AM_DEVWRITE("pcggout", spc1500_state, pcgg_w)
	AM_RANGE(0x1800, 0x1801) AM_DEVREADWRITE("crtc", spc1500_state, crtc_r, crtc_w)
	AM_RANGE(0x1900, 0x1909) AM_READ(keyboard_r)
// 	AM_RANGE(0x1a00, 0x1a03) AM_DEVREADWRITE("i8255", m_pio, data_r, data_w)
	AM_RANGE(0x1b00, 0x1b00) AM_DEVREADWRITE("ay8910", ay8910_device, data_r, data_w)
	AM_RANGE(0x1c00, 0x1c00) AM_DEVWRITE("ay8910", ay8910_device, address_w)
	AM_RANGE(0x1d00, 0x1d00) AM_WRITE(romsel)
	AM_RANGE(0x1e00, 0x1e00) AM_WRITE(ramsel)
	AM_RANGE(0x2000, 0xffff) AM_RAM AM_SHARE("videoram")
ADDRESS_MAP_END

/* Input ports */
static INPUT_PORTS_START( spc1500 )
	PORT_START("LINE.0")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Shift") PORT_CODE(KEYCODE_RSHIFT) PORT_CODE(KEYCODE_LSHIFT) PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Ctrl") PORT_CODE(KEYCODE_RCONTROL) PORT_CODE(KEYCODE_LCONTROL) PORT_CHAR(UCHAR_SHIFT_2)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Break") PORT_CODE(KEYCODE_PAUSE)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Graph") PORT_CODE(KEYCODE_LALT) PORT_CODE(KEYCODE_RALT)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("LINE.1")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("^ ~") PORT_CODE(KEYCODE_TILDE) PORT_CHAR('^') PORT_CHAR('~') PORT_CHAR(0x1e)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Home") PORT_CODE(KEYCODE_HOME)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Space") PORT_CODE(KEYCODE_SPACE) PORT_CHAR(' ')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Return") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(13)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("C") PORT_CODE(KEYCODE_C) PORT_CHAR('c') PORT_CHAR('C') PORT_CHAR(0x03)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("A") PORT_CODE(KEYCODE_A) PORT_CHAR('a') PORT_CHAR('A') PORT_CHAR(0x01)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Q") PORT_CODE(KEYCODE_Q) PORT_CHAR('q') PORT_CHAR('Q') PORT_CHAR(0x16)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("1 !") PORT_CODE(KEYCODE_1) PORT_CHAR('1') PORT_CHAR('!')

	PORT_START("LINE.2")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Caps") PORT_CODE(KEYCODE_CAPSLOCK)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Z") PORT_CODE(KEYCODE_Z) PORT_CHAR('z') PORT_CHAR('Z') PORT_CHAR(0x1a)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("] }") PORT_CODE(KEYCODE_CLOSEBRACE) PORT_CHAR(']') PORT_CHAR('}') PORT_CHAR(0x1d)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("V") PORT_CODE(KEYCODE_V) PORT_CHAR('v') PORT_CHAR('V') PORT_CHAR(0x16)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("S") PORT_CODE(KEYCODE_S) PORT_CHAR('s') PORT_CHAR('S') PORT_CHAR(0x13)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("W") PORT_CODE(KEYCODE_W) PORT_CHAR('w') PORT_CHAR('W') PORT_CHAR(0x17)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("2 \"") PORT_CODE(KEYCODE_2) PORT_CHAR('2') PORT_CHAR('\"')

	PORT_START("LINE.3")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Del") PORT_CODE(KEYCODE_BACKSPACE) PORT_CHAR(8) PORT_CHAR(0x12)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("ESC") PORT_CODE(KEYCODE_ESC) PORT_CHAR(0x1b)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("[ {") PORT_CODE(KEYCODE_OPENBRACE) PORT_CHAR('[') PORT_CHAR('{') PORT_CHAR(0x1b)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("B") PORT_CODE(KEYCODE_B) PORT_CHAR('b') PORT_CHAR('B') PORT_CHAR(0x02)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("D") PORT_CODE(KEYCODE_D) PORT_CHAR('d') PORT_CHAR('D') PORT_CHAR(0x04)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("E") PORT_CODE(KEYCODE_E) PORT_CHAR('e') PORT_CHAR('E') PORT_CHAR(0x05)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("3 #") PORT_CODE(KEYCODE_3) PORT_CHAR('3') PORT_CHAR('#')

	PORT_START("LINE.4")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Right") PORT_CODE(KEYCODE_RIGHT)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("\\ |") PORT_CODE(KEYCODE_BACKSLASH) PORT_CHAR('\\') PORT_CHAR('|') PORT_CHAR(0x1c)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("N") PORT_CODE(KEYCODE_N) PORT_CHAR('n') PORT_CHAR('N') PORT_CHAR(0x0e)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F") PORT_CODE(KEYCODE_F) PORT_CHAR('f') PORT_CHAR('F') PORT_CHAR(0x06)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("R") PORT_CODE(KEYCODE_R) PORT_CHAR('r') PORT_CHAR('R') PORT_CHAR(0x12)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("4 $") PORT_CODE(KEYCODE_4) PORT_CHAR('4') PORT_CHAR('$')

	PORT_START("LINE.5")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F1") PORT_CODE(KEYCODE_F1)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Left") PORT_CODE(KEYCODE_LEFT)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("M") PORT_CODE(KEYCODE_M) PORT_CHAR('m') PORT_CHAR('M') PORT_CHAR(0x0d)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("G") PORT_CODE(KEYCODE_G) PORT_CHAR('g') PORT_CHAR('G') PORT_CHAR(0x07)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("T") PORT_CODE(KEYCODE_T) PORT_CHAR('t') PORT_CHAR('T') PORT_CHAR(0x14)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("5 %") PORT_CODE(KEYCODE_5) PORT_CHAR('5') PORT_CHAR('%')

	PORT_START("LINE.6")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F2") PORT_CODE(KEYCODE_F2)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("@ `") PORT_CODE(KEYCODE_EQUALS) PORT_CHAR('@') PORT_CHAR('`')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("X") PORT_CODE(KEYCODE_X) PORT_CHAR('x') PORT_CHAR('X') PORT_CHAR(0x18)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(", <") PORT_CODE(KEYCODE_COMMA) PORT_CHAR(',') PORT_CHAR('<')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("H") PORT_CODE(KEYCODE_H) PORT_CHAR('h') PORT_CHAR('H') PORT_CHAR(0x08)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Y") PORT_CODE(KEYCODE_Y) PORT_CHAR('y') PORT_CHAR('Y') PORT_CHAR(0x19)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("6 &") PORT_CODE(KEYCODE_6) PORT_CHAR('6') PORT_CHAR('&')

	PORT_START("LINE.7")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F3") PORT_CODE(KEYCODE_F3)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Up") PORT_CODE(KEYCODE_UP)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("P") PORT_CODE(KEYCODE_P) PORT_CHAR('p') PORT_CHAR('P') PORT_CHAR(0x10)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(". >") PORT_CODE(KEYCODE_STOP) PORT_CHAR('.') PORT_CHAR('>')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("J") PORT_CODE(KEYCODE_J) PORT_CHAR('j') PORT_CHAR('J') PORT_CHAR(0x0a)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("U") PORT_CODE(KEYCODE_U) PORT_CHAR('u') PORT_CHAR('U') PORT_CHAR(0x15)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("7 '") PORT_CODE(KEYCODE_7) PORT_CHAR('7') PORT_CHAR('\'')

	PORT_START("LINE.8")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F4") PORT_CODE(KEYCODE_F4)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Down") PORT_CODE(KEYCODE_DOWN)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(": *") PORT_CODE(KEYCODE_QUOTE) PORT_CHAR(':') PORT_CHAR('*')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("/ ?") PORT_CODE(KEYCODE_SLASH) PORT_CHAR('/') PORT_CHAR('?')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K") PORT_CODE(KEYCODE_K) PORT_CHAR('k') PORT_CHAR('K') PORT_CHAR(0x0b)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("I") PORT_CODE(KEYCODE_I) PORT_CHAR('i') PORT_CHAR('I') PORT_CHAR(0x09)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("8 (") PORT_CODE(KEYCODE_8) PORT_CHAR('8') PORT_CHAR('(')

	PORT_START("LINE.9")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_START)    PORT_NAME("IPL") PORT_CODE(KEYCODE_END)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F5") PORT_CODE(KEYCODE_F5)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("- =") PORT_CODE(KEYCODE_MINUS) PORT_CHAR('-') PORT_CHAR('=')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("0") PORT_CODE(KEYCODE_0) PORT_CHAR('0')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("; +") PORT_CODE(KEYCODE_COLON) PORT_CHAR(';') PORT_CHAR('+')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("L") PORT_CODE(KEYCODE_L) PORT_CHAR('l') PORT_CHAR('L') PORT_CHAR(0x0c)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("O") PORT_CODE(KEYCODE_O) PORT_CHAR('o') PORT_CHAR('O') PORT_CHAR(0x0e)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("9 )") PORT_CODE(KEYCODE_9) PORT_CHAR('9') PORT_CHAR(')')

	PORT_START("JOY")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_UNUSED) // Button 2?
	PORT_BIT(0xc0, IP_ACTIVE_HIGH, IPT_UNUSED) // Cassette related
INPUT_PORTS_END


void spc1500_state::machine_start()
{
	UINT8 *mem_ipl = memregion("ipl")->base();
	UINT8 *mem_basic = memregion("basic")->base();
	UINT8 *ram = m_ram->pointer();
	m_cg_rom = memregion("cgrom")->base();	

	// configure and intialize banks 1 & 3 (read banks)
	membank("bank1")->configure_entry(0, ram);
	membank("bank1")->configure_entry(1, mem_ipl);
	membank("bank1")->configure_entry(2, mem_basic);
	membank("bank1")->set_entry(1);

	// intialize banks 2, 3, 4 (write banks)
	membank("bank2")->set_base(ram);
	membank("bank3")->set_base(ram + 0x8000);
	membank("bank4")->set_base(ram + 0x8000);
	
   	m_time = machine().scheduler().time();	
}

void spc1500_state::machine_reset()
{
	m_work_ram = auto_alloc_array_clear(machine(), UINT8, 0x10000);
}

READ8_MEMBER(spc1500_state::mc6845_videoram_r)
{
	return m_p_videoram[offset];
}

READ8_MEMBER( spc1500_state::porta_r )
{
	UINT8 data = 0x3f;
	data |= (m_cass->input() > 0.0038) ? 0x80 : 0;
	data |= ((m_cass->get_state() & CASSETTE_MASK_UISTATE) != CASSETTE_STOPPED) && ((m_cass->get_state() & CASSETTE_MASK_MOTOR) == CASSETTE_MOTOR_ENABLED)  ? 0x00 : 0x40;
	data &= ~(m_io_joy->read() & 0x3f);
	data &= ~((m_centronics_busy == 0)<< 5);
	return data;
}

// irq is inverted in emulation, so we need this trampoline
WRITE_LINE_MEMBER( spc1500_state::irq_w )
{
	m_maincpu->set_input_line(0, state ? CLEAR_LINE : HOLD_LINE);
}

PALETTE_INIT_MEMBER(spc1500_state,spc)
{
	int i;

	for(i=0;i<(0x10+0x1000);i++)
		palette.set_pen_color(i,rgb_t(0x00,0x00,0x00));
}

// /* decoded for debugging purpose, this will be nuked in the end... */
// static GFXDECODE_START( x1 )
	// GFXDECODE_ENTRY( "cgrom",   0x00000, x1_chars_8x8,    0, 1 )
	// GFXDECODE_ENTRY( "font",    0x00000, x1_chars_8x16,   0, 1 )
	// GFXDECODE_ENTRY( "kanji",   0x00000, x1_chars_16x16,  0, 1 )
// GFXDECODE_ENTRY( "pcg",     0x00000, x1_pcg_8x8,      0, 1 )
// GFXDECODE_END

//-------------------------------------------------
//  address maps
//-------------------------------------------------

static MACHINE_CONFIG_START( spc1500, spc1500_state )
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu",Z80, XTAL_4MHz)
	MCFG_CPU_PROGRAM_MAP(spc1500_mem)
	MCFG_CPU_IO_MAP(spc1500_io)

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_RAW_PARAMS(XTAL_14_31818MHz,912,0,640,262,0,200)
	MCFG_SCREEN_UPDATE_DRIVER( spc1500_state, screen_update )

	MCFG_PALETTE_ADD("palette", /* CGA_PALETTE_SETS * 16*/ 65536 )

#if 1	
	MCFG_MC6845_ADD("crtc", H46505, "screen", (VDP_CLOCK/48)) //unknown divider
	MCFG_MC6845_SHOW_BORDER_AREA(true)
	MCFG_MC6845_CHAR_WIDTH(8)
	MCFG_PALETTE_ADD("palette", 0x10+0x1000)
	MCFG_PALETTE_INIT_OWNER(spc1500_state, spc)

	//MCFG_GFXDECODE_ADD("gfxdecode", "palette", spc)

	MCFG_VIDEO_START_OVERRIDE(spc1500_state, spc)	
#else	
	MCFG_MC6845_ADD("crtc", MC6845, "screen", XTAL_14_31818MHz/8)
	MCFG_MC6845_SHOW_BORDER_AREA(false)
	MCFG_MC6845_CHAR_WIDTH(8)
	MCFG_MC6845_UPDATE_ROW_CB(spc1500_state, crtc_update_row)
	MCFG_MC6845_OUT_HSYNC_CB(WRITELINE(spc1500_state, hsync_changed))
	MCFG_MC6845_OUT_VSYNC_CB(WRITELINE(spc1500_state, vsync_changed))
	MCFG_MC6845_RECONFIGURE_CB(spc1500_state, reconfigure)
	MCFG_VIDEO_SET_SCREEN(nullptr)
#endif
	
	MCFG_DEVICE_ADD("ppi8255", I8255, 0)
	MCFG_I8255_OUT_PORTA_CB(WRITE8(spc1500_state, porta_w))
	MCFG_I8255_IN_PORTB_CB(READ8(spc1500_state, portb_r))
	MCFG_I8255_OUT_PORTB_CB(WRITE8(spc1500_state, portb_w))
	MCFG_I8255_OUT_PORTC_CB(WRITE8(spc1500_state, portc_w))
	
	// other lines not connected

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("ay8910", AY8910, XTAL_4MHz / 2)
	MCFG_AY8910_PORT_A_READ_CB(READ8(spc1500_state, porta_r))
	MCFG_AY8910_PORT_B_WRITE_CB(DEVWRITE8("cent_data_out", output_latch_device, write))
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)
	MCFG_SOUND_WAVE_ADD(WAVE_TAG, "cassette")
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)

	MCFG_CENTRONICS_ADD("centronics", centronics_devices, "printer")
	MCFG_CENTRONICS_BUSY_HANDLER(WRITELINE(spc1500_state, centronics_busy_w))
	MCFG_CENTRONICS_OUTPUT_LATCH_ADD("cent_data_out", "centronics")
	MCFG_DEVICE_ADD("cent_status_in", INPUT_BUFFER, 0)

	MCFG_CASSETTE_ADD("cassette")
	MCFG_CASSETTE_FORMATS(spc1000_cassette_formats)
	MCFG_CASSETTE_DEFAULT_STATE(CASSETTE_STOPPED | CASSETTE_SPEAKER_ENABLED | CASSETTE_MOTOR_DISABLED)

	MCFG_SOFTWARE_LIST_ADD("cass_list", "spc1500_cass")

	/* internal ram */
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("64K")
MACHINE_CONFIG_END

/* ROM definition */
ROM_START( spc1500 )
	ROM_REGION(0x1000, "ipl", ROMREGION_ERASEFF)
	ROM_LOAD("ipl.rom", 0x0000, 0x8000, CRC(19638fc9) SHA1(489f1baa7aebf3c8c660325fb1fd790d84203284))
	ROM_REGION(0x2000, "basic", ROMREGION_ERASEFF)
	ROM_LOAD("basic.rom", 0x0000, 0x8000, CRC(19638fc9) SHA1(489f1baa7aebf3c8c660325fb1fd790d84203284))
	ROM_REGION(0x10000, "font1", 0) 
	ROM_LOAD( "ss150fnt.bin", 0x0000, 0x2000, CRC(19689fbd) SHA1(0d4e072cd6195a24a1a9b68f1d37500caa60e599) )
	ROM_REGION(0x12000, "font2", 0) 
	ROM_LOAD( "ss151fnt.bin", 0x2000, 0x2000, CRC(19689fbd) SHA1(0d4e072cd6195a24a1a9b68f1d37500caa60e599) )
	ROM_REGION(0x14000, "font3", 0) 
	ROM_LOAD( "ss152fnt.bin", 0x4000, 0x2000, CRC(19689fbd) SHA1(0d4e072cd6195a24a1a9b68f1d37500caa60e599) )
	ROM_REGION(0x16000, "font4", 0) 
	ROM_LOAD( "ss153fnt.bin", 0x6000, 0x2000, CRC(19689fbd) SHA1(0d4e072cd6195a24a1a9b68f1d37500caa60e599) )
	
ROM_END


/* Driver */

/*    YEAR  NAME      PARENT  COMPAT   MACHINE    INPUT    CLASS         INIT    COMPANY    FULLNAME       FLAGS */
COMP( 1984, spc1500,  0,      0,       spc1500,   spc1500, driver_device,  0,   "Samsung", "SPC-1500", MACHINE_NOT_WORKING )
