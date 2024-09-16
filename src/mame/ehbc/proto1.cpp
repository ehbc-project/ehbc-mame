// license: BSD-3-Clause
/****************************************************************************

    EHBC proto1

    Hardware:
    - MC68030
    - MX8315 Clock Generator
	- FPM/EDO DRAM up to 64 MiB

    TODO:
	

****************************************************************************/

#include "emu.h"
#include "bus/rs232/rs232.h"
#include "bus/isa/isa.h"
#include "bus/isa/isa_cards.h"
#include "bus/ata/hdd.h"
#include "cpu/m68000/m68030.h"
#include "machine/idectrl.h"
#include "machine/mc68901.h"
#include "machine/mc68681.h"
#include "machine/mc146818.h"
#include "machine/keyboard.h"
#include "machine/8042kbdc.h"
#include "machine/ram.h"
#include "machine/upd765.h"
#include "video/mc6845.h"
#include "video/pc_vga.h"
#include "video/pc_vga_cirrus.h"
#include "imagedev/floppy.h"

namespace {


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class proto1_state : public driver_device
{
public:
	proto1_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_ram(*this, RAM_TAG),
		m_mfp(*this, "mfp%u", 0U),
		m_duart(*this, "duart"),
		m_serial(*this, "serial%u", 0U),
		m_ide(*this, "ide"),
		m_flash(*this, "flash"),
		m_vga(*this, "vga"),
		m_fdc(*this, "fdc"),
		m_rtc(*this, "rtc"),
		m_switches(*this, "switches")
	{ }

	void proto1(machine_config &config);

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

private:
	required_device<m68030_device> m_maincpu;
	required_device<ram_device> m_ram;
	required_device_array<mc68901_device, 2> m_mfp;
	required_device<mc68681_device> m_duart;
	required_device_array<rs232_port_device, 2> m_serial;
	required_device<ide_controller_device> m_ide;
	required_memory_region m_flash;
	required_device<vga_device> m_vga;
	required_device<pc8477b_device> m_fdc;
	required_device<mc146818_device> m_rtc;
	required_ioport m_switches;

	void mem_map(address_map &map);
};


//**************************************************************************
//  ADDRESS MAPS
//**************************************************************************

void proto1_state::mem_map(address_map &map)
{
	map(0x00000000, 0xFBFFFFFF).ram();

	map(0xFD000000, 0xFDFFFFFF).rom().region("flash", 0);

	// pc i/o ports
	map(0xFE000060, 0xFE000064).rw("kbdc", FUNC(kbdc8042_device::data_r), FUNC(kbdc8042_device::data_w));
	map(0xFE000070, 0xFE000070).w(m_rtc, FUNC(mc146818_device::address_w));
	map(0xFE000071, 0xFE000071).rw(m_rtc, FUNC(mc146818_device::data_r), FUNC(mc146818_device::data_w));
	map(0xFE0001F0, 0xFE0001F7).rw(m_ide, FUNC(ide_controller_device::cs0_r), FUNC(ide_controller_device::cs0_w));
	map(0xFE0003B0, 0xFE0003DF).m(m_vga, FUNC(cirrus_gd5428_vga_device::io_map));
	map(0xFE0003F0, 0xFE0003F7).m(m_fdc, FUNC(pc8477b_device::map));
	map(0xFE0003F0, 0xFE0003F7).rw(m_ide, FUNC(ide_controller_device::cs1_r), FUNC(ide_controller_device::cs1_w));

	// pc memory map (~16MiB)
	map(0xFE0A0000, 0xFE0BFFFF).rw(m_vga, FUNC(cirrus_gd5428_vga_device::mem_r), FUNC(cirrus_gd5428_vga_device::mem_w));
	map(0xFE100000, 0xFE1FFFFF).rw(m_vga, FUNC(cirrus_gd5446_vga_device::mem_linear_r), FUNC(cirrus_gd5446_vga_device::mem_linear_w));

	// mmio
	map(0xFF000000, 0xFF0000FF);  // for chipset
	map(0xFF000100, 0xFF00010F).rw(m_mfp[0], FUNC(mc68901_device::read), FUNC(mc68901_device::write));
	map(0xFF000110, 0xFF00011F).rw(m_mfp[1], FUNC(mc68901_device::read), FUNC(mc68901_device::write));
	map(0xFF000200, 0xFF00020F).rw(m_duart, FUNC(mc68681_device::read), FUNC(mc68681_device::write));
	map(0xFF000300, 0xFF0003FF);  // mc68440 #0
	map(0xFF000400, 0xFF0004FF);  // mc68440 #1
}

//**************************************************************************
//  INPUT DEFINITIONS
//**************************************************************************

static INPUT_PORTS_START( proto1 )
	PORT_START("switches")
	PORT_DIPNAME(0x01, 0x01, "IO Mode")
	PORT_DIPLOCATION("DIL:1")
	PORT_DIPSETTING(   0x00, "Terminal")
	PORT_DIPSETTING(   0x01, "Internal")
	PORT_DIPNAME(0x02, 0x02, "Columns")
	PORT_DIPLOCATION("DIL:2")
	PORT_DIPSETTING(   0x00, "40")
	PORT_DIPSETTING(   0x02, "80")
	// DIL:3 and DIL:4 select parallel keyboard strobe polarity
INPUT_PORTS_END

//**************************************************************************
//  MACHINE EMULATION
//**************************************************************************

void proto1_state::machine_start()
{
	machine_reset();
}

void proto1_state::machine_reset()
{
	m_maincpu->space(0).write_dword(0, 0x00000000);
	m_maincpu->space(0).write_dword(4, 0xFD000000);
}


//**************************************************************************
//  MACHINE DEFINTIONS
//**************************************************************************

void proto1_state::proto1(machine_config &config)
{
	M68030(config, m_maincpu, 25_MHz_XTAL);
	m_maincpu->set_addrmap(AS_PROGRAM, &proto1_state::mem_map);

	RAM(config, RAM_TAG)
		.set_default_size("512K")
		.set_extra_options("1M,2M,4M,8M,16M,32M,64M");

	MC68901(config, m_mfp[0], 16_MHz_XTAL / 4);

	MC68901(config, m_mfp[1], 16_MHz_XTAL / 4);

	screen_device &screen(SCREEN(config, "screen", SCREEN_TYPE_RASTER));
	screen.set_raw(25.175_MHz_XTAL, 800, 0, 640, 525, 0, 480);
	screen.set_screen_update(m_vga, FUNC(cirrus_gd5428_vga_device::screen_update));

	CIRRUS_GD5428_VGA(config, m_vga, 0);
	m_vga->set_screen("screen");
	m_vga->set_vram_size(0x200000);

	IDE_CONTROLLER(config, m_ide);
	m_ide->options(ata_devices, "hdd", nullptr, false);

	MC68681(config, m_duart, 16_MHz_XTAL / 2);
	m_duart->a_tx_cb().set(m_serial[0], FUNC(rs232_port_device::write_txd));
	m_duart->outport_cb().append(m_serial[0], FUNC(rs232_port_device::write_rts)).bit(0);
	m_duart->b_tx_cb().set(m_serial[1], FUNC(rs232_port_device::write_txd));
	m_duart->outport_cb().append(m_serial[1], FUNC(rs232_port_device::write_rts)).bit(1);

	RS232_PORT(config, m_serial[0], default_rs232_devices, "terminal");
	m_serial[0]->rxd_handler().set(m_duart, FUNC(mc68681_device::rx_a_w));
	m_serial[0]->cts_handler().set(m_duart, FUNC(mc68681_device::ip0_w));

	RS232_PORT(config, m_serial[1], default_rs232_devices, nullptr);
	m_serial[1]->rxd_handler().set(m_duart, FUNC(mc68681_device::rx_b_w));
	m_serial[1]->cts_handler().set(m_duart, FUNC(mc68681_device::ip1_w));

	PC8477B(config, m_fdc, 24_MHz_XTAL, pc8477b_device::mode_t::PS2);

	floppy_connector &fdconn0(FLOPPY_CONNECTOR(config, "fdc:0"));
	fdconn0.option_add("35hd", FLOPPY_35_HD);
	fdconn0.option_add("35dd", FLOPPY_35_DD);
	fdconn0.option_add("525hd", FLOPPY_525_HD);
	fdconn0.option_add("525dd", FLOPPY_525_DD);
	fdconn0.set_default_option("35hd");
	fdconn0.set_formats(floppy_image_device::default_pc_floppy_formats);

	floppy_connector &fdconn1(FLOPPY_CONNECTOR(config, "fdc:1"));
	fdconn1.option_add("35hd", FLOPPY_35_HD);
	fdconn1.option_add("35dd", FLOPPY_35_DD);
	fdconn1.option_add("525hd", FLOPPY_525_HD);
	fdconn1.option_add("525dd", FLOPPY_525_DD);
	fdconn1.set_default_option("35hd");
	fdconn1.set_formats(floppy_image_device::default_pc_floppy_formats);

	kbdc8042_device &kbdc(KBDC8042(config, "kbdc"));
	kbdc.set_keyboard_type(kbdc8042_device::KBDC8042_STANDARD);
	kbdc.system_reset_callback().set_inputline(m_maincpu, INPUT_LINE_RESET);
	kbdc.set_keyboard_tag("at_keyboard");

	MC146818(config, m_rtc, 32.768_kHz_XTAL);

	// at_keyboard_device &at_keyb(AT_KEYB(config, "at_keyboard", pc_keyboard_device::KEYBOARD_TYPE::AT, 1));
	// at_keyb.keypress().set("kbdc", FUNC(kbdc8042_device::keyboard_w));
}


//**************************************************************************
//  ROM DEFINITIONS
//**************************************************************************

ROM_START( proto1 )
	ROM_REGION32_BE(0x1000000, "flash", 0)
	ROM_DEFAULT_BIOS("v0")
	ROM_SYSTEM_BIOS(0, "v0",  "Dev")
	ROM_LOAD("proto1_firmware.bin", 0, 0x100000, "")
ROM_END


} // anonymous namespace


//**************************************************************************
//  SYSTEM DRIVERS
//**************************************************************************

//    YEAR  NAME    PARENT  COMPAT  MACHINE INPUT   CLASS         INIT        COMPANY    FULLNAME       FLAGS
COMP( 2024, proto1, 0,      0,      proto1, proto1, proto1_state, empty_init, "kms1212", "EHBC Proto1", MACHINE_IMPERFECT_GRAPHICS | MACHINE_NO_SOUND | MACHINE_SUPPORTS_SAVE )
