// license:BSD-3-Clause
// copyright-holders:Fabio Priuli
#ifndef MAME_BUS_SPC1000_RPIBOX_H
#define MAME_BUS_SPC1000_RPIBOX_H

#pragma once

#include "exp.h"

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> spc1000_vdp_exp_device

class spc1000_rpibox_exp_device : public device_t, public device_spc1000_card_interface
{
public:
	// construction/destruction
	spc1000_rpibox_exp_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_add_mconfig(machine_config &config) override;

	virtual DECLARE_READ8_MEMBER(read) override;
	virtual DECLARE_WRITE8_MEMBER(write) override;

	DECLARE_WRITE_LINE_MEMBER(rpibox_interrupt);

private:
	// internal state
};


// device type definition
DECLARE_DEVICE_TYPE(SPC1000_RPIBOX_EXP, spc1000_rpibox_exp_device)

#endif // MAME_BUS_SPC1000_VDP_H 