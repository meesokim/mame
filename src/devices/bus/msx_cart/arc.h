// license:BSD-3-Clause
// copyright-holders:Wilbert Pol
#ifndef __MSX_CART_ARC_H
#define __MSX_CART_ARC_H

#include "bus/msx_cart/cartridge.h"


extern const device_type MSX_CART_ARC;


class msx_cart_arc : public device_t
					, public msx_cart_interface
{
public:
	msx_cart_arc(const machine_config &mconfig, std::string tag, device_t *owner, UINT32 clock);

	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

	virtual void initialize_cartridge() override;

	virtual DECLARE_READ8_MEMBER(read_cart) override;

	DECLARE_WRITE8_MEMBER(io_7f_w);
	DECLARE_READ8_MEMBER(io_7f_r);

private:
	UINT8 m_7f;
};


#endif
