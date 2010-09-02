/*
 * usb.c - NXT TVM USB interface
 *
 * Copyright (C) 2007  the NxOS developers (see NXOS)
 * Copyright (C) 2010  Carl G. Ritson
 *
 * Redistribution of this file is permitted under
 * the terms of the GNU Public License (GPL) version 2.
 */

#include "tvm-nxt.h"
#include "at91sam7s256.h"

#ifndef MIN
#define MIN(x,y)  (((x) <= (y)) ? (x) : (y))
#endif /* MIN */

/* The USB controller supports up to 4 endpoints. */
#define N_ENDPOINTS 4

/* Maximum data packet sizes. Endpoint 0 is a special case (control
 * endpoint).
 */
#define MAX_EP0_SIZE 8
#define MAX_RCV_SIZE 64
#define MAX_SND_SIZE 64


/* Various constants for the setup packets.
 */
#define USB_BMREQUEST_DIR		0x80
#define USB_BMREQUEST_H_TO_D		0x00
#define USB_BMREQUEST_D_TO_H		0x80

#define USB_BMREQUEST_RCPT		0x0F
#define USB_BMREQUEST_RCPT_DEV		0x00 /* device */
#define USB_BMREQUEST_RCPT_INT		0x01 /* interface */
#define USB_BMREQUEST_RCPT_EPT		0x02 /* endpoint */
#define USB_BMREQUEST_RCPT_OTH		0x03 /* other */

#define USB_BMREQUEST_CLASS		0x20

#define USB_BREQUEST_GET_STATUS		0x00
#define USB_BREQUEST_CLEAR_FEATURE	0x01
#define USB_BREQUEST_SET_FEATURE	0x03
#define USB_BREQUEST_SET_ADDRESS	0x05
#define USB_BREQUEST_GET_DESCRIPTOR	0x06
#define USB_BREQUEST_SET_DESCRIPTOR	0x07
#define USB_BREQUEST_GET_CONFIG		0x08
#define USB_BREQUEST_SET_CONFIG		0x09
#define USB_BREQUEST_GET_INTERFACE	0x0A
#define USB_BREQUEST_SET_INTERFACE	0x0B

#define USB_BREQUEST_BULK_RESET		0xFF
#define USB_BREQUEST_GET_MAX_LUN	0xFE

#define USB_WVALUE_TYPE			(0xFF << 8)
#define USB_DESC_TYPE_DEVICE		1
#define USB_DESC_TYPE_CONFIG		2
#define USB_DESC_TYPE_STR		3
#define USB_DESC_TYPE_INT		4
#define USB_DESC_TYPE_ENDPT		5
#define USB_DESC_TYPE_DEVICE_QUALIFIER	6

#define USB_WVALUE_INDEX		0xFF

#define USB_FEAT_ENDPOINT_HALT		0

/* The following definitions are 'raw' USB setup packets. They are all
 * standard responses to various setup requests by the USB host. These
 * packets are all constant, and mostly boilerplate. Don't be too
 * bothered if you skip over these to real code.
 *
 * If you want to understand the full meaning of every bit of these
 * packets, you should refer to the USB 2.0 specifications.
 *
 * One point of interest: the USB device space is partitionned by
 * vendor and product ID. As we are lacking money and real need, we
 * don't have a vendor ID to use. Therefore, we are currently
 * piggybacking on Lego's device space, using an unused product ID.
 */
static const uint8_t usb_device_descriptor[] = {
	18, USB_DESC_TYPE_DEVICE, /* Packet size and type. */
	0x00, 0x20, /* This packet is USB 2.0. */
	0, /* Class code. */
	0, /* Sub class code. */
	0, /* Device protocol. */
	MAX_EP0_SIZE, /* Maximum packet size for EP0 (control endpoint). */
	0x94, 0x06, /* Vendor ID : LEGO */
	0x00, 0xFF, /* Product ID : NXOS */
	0x00, 0x00, /* Product revision. */
	1, /* Index of the vendor string. */
	2, /* Index of the product string. */
	0, /* Index of the serial number (none for us). */
	1, /* The number of possible configurations. */
};

static const uint8_t usb_dev_qualifier_desc[] = {
	10, USB_DESC_TYPE_DEVICE_QUALIFIER, /* Packet size and type. */
	0x00, 0x20, /* This packet is USB 2.0. */
	0, /* Class code */
	0, /* Sub class code */
	0, /* Device protocol */
	MAX_EP0_SIZE, /* Maximum packet size for EP0. */
	1, /* The number of possible configurations. */
	0 /* Reserved for future use, must be zero. */
};


static const uint8_t usb_full_config[] = {
	0x09, USB_DESC_TYPE_CONFIG, /* Descriptor size and type. */
	0x20, 0x00, /* Total length of the configuration, interface
		     * description included.
		     */
	1, /* The number of interfaces declared by this configuration. */
	1, /* The ID for this configuration. */
	0, /* Index of the configuration description string (none). */

	/* Configuration attributes bitmap. Bit 7 (MSB) must be 1, bit 6 is
	 * 1 because the NXT is self-powered, bit 5 is 0 because the NXT
	 * doesn't support remote wakeup, and bits 0-4 are 0 (reserved).
	 */
	0x40,
	0, /* Device power consumption, for non self-powered devices. */


	/*
	 * This is the descriptor for the interface associated with the
	 * configuration.
	 */
	0x09, USB_DESC_TYPE_INT, /* Descriptor size and type. */
	0x00, /* Interface index. */
	0x00, /* ID for this interface configuration. */
	0x02, /* The number of endpoints defined by this interface
	       * (excluding EP0).
	       */
	0x08, /* Interface class ("MASS STORAGE"). */
	0x06, /* Interface subclass ("SCSI transparent"). */
	0x50, /* Interface protocol ("BULK-ONLY TRANSPORT"). */
	0x00, /* Index of the string descriptor for this interface (none). */


	/*
	 * Descriptor for EP1.
	 */
	7, USB_DESC_TYPE_ENDPT, /* Descriptor length and type. */
	0x01, /* Endpoint number. MSB is zero, meaning this is an OUT EP. */
	0x02, /* Endpoint type (bulk). */
	MAX_RCV_SIZE, 0x00, /* Maximum packet size (64). */
	0, /* EP maximum NAK rate (device never NAKs). */


	/*
	 * Descriptor for EP2.
	 */
	7, USB_DESC_TYPE_ENDPT, /* Descriptor length and type. */
	0x82, /* Endpoint number. MSB is one, meaning this is an IN EP. */
	0x02, /* Endpoint type (bulk). */
	MAX_RCV_SIZE, 0x00, /* Maximum packet size (64). */
	0, /* EP maximum NAK rate (device never NAKs). */
};


static const uint8_t usb_string_desc[] = {
	4, USB_DESC_TYPE_STR, /* Descriptor length and type. */
	0x09, 0x04, /* Supported language ID (US English). */
};

static const uint8_t usb_lego_str[] = {
	10, USB_DESC_TYPE_STR,
	'L', 0,
	'E', 0,
	'G', 0,
	'O', 0
};

static const uint8_t usb_tvm_str[] = {
	16, USB_DESC_TYPE_STR,
	'N', 0,
	'X', 0,
	'T', 0,
	' ', 0,
	'T', 0,
	'V', 0,
	'M', 0
};

/* Internal lookup table mapping string descriptors to their indices
 * in the USB string descriptor table.
 */
static const uint8_t *usb_strings[] = {
	usb_lego_str,
	usb_tvm_str
};
static const uint8_t usb_string_count = 
	(uint8_t) (sizeof (usb_strings) / sizeof (uint8_t *));

/*
 * The USB device state. Contains the current USB state (selected
 * configuration, etc.) and transitory state for data transfers.
 */
static volatile struct {
	/* The current state of the device. */
	enum usb_status {
		USB_UNINITIALISED = 0,
		USB_READY,
		USB_BUSY,
		USB_SUSPENDED,
	} status;

	/* Holds the status the bus was in before entering suspend. */
	enum usb_status pre_suspend_status;

	/* When the host gives us an address, we must send a null ACK packet
	 * back before actually changing addresses. This field stores the
	 * address that should be set once the ACK is sent.
	 */
	uint32_t new_device_address;

	/* The currently selected USB configuration. */
	uint8_t current_config;
	
	/* Bitmap of endpoint halt bits */
	uint8_t halted;

	/* Holds the state of the data transmissions on endpoints.
	 * This only gets used if the transmission needed to be split
	 * into several USB packets.
	 * 0 = EP0
	 * 1 = EP2
	 */
	uint8_t *tx_data[2];
	uint32_t tx_len[2];

	/* Used to write the data from the EP1
	*/
	uint8_t *rx_data;
	/* size of the rx data buffer */
	uint32_t rx_size;
	/* length of the read packet (0 if none) */
	uint32_t rx_len;

	/* The USB controller has two hardware input buffers. This remembers
	 * the one currently in use.
	 */
	uint8_t current_rx_bank;
} usb_state;


/* The flags in the UDP_CSR register are a little strange: writing to
 * them does not instantly change their value. Their value will change
 * to reflect the write when the USB controller has taken the change
 * into account. The driver must wait until the controller
 * acknowledges changes to CSR.
 *
 * These helpers set/clear CSR flags, and then loop waiting for the
 * controller to synchronize
 */
static inline void usb_csr_clear_flag (uint8_t endpoint, uint32_t flags)
{
	AT91C_UDP_CSR[endpoint] &= ~(flags);
	while (AT91C_UDP_CSR[endpoint] & (flags));
}

static inline void usb_csr_set_flag (uint8_t endpoint, uint32_t flags)
{
	AT91C_UDP_CSR[endpoint] |= (flags);
	while ((AT91C_UDP_CSR[endpoint] & (flags)) != (flags));
}


/* Starts sending data to the host. If the data cannot fit into a
 * single USB packet, the data is split and scheduled to be sent in
 * several packets.
 */
static void usb_write_data (int endpoint, const uint8_t *ptr, uint32_t length)
{
	uint32_t packet_size;
	int tx;

	if (endpoint != 0 && endpoint != 2)
		return;

	tx = endpoint / 2;

	/* The bus is now busy. */
	usb_state.status = USB_BUSY;

	if (endpoint == 0)
		packet_size = MIN (MAX_EP0_SIZE, length);
	else
		packet_size = MIN (MAX_SND_SIZE, length);

	/* If there is more data than can fit in a single packet, queue the
	 * rest up.
	 */
	if (length > packet_size) {
		length -= packet_size;
		usb_state.tx_data[tx] = (uint8_t*)(ptr + packet_size);
		usb_state.tx_len[tx] = length;
	} else {
		usb_state.tx_data[tx] = NULL;
		usb_state.tx_len[tx] = 0;
	}

	/* Push a packet into the USB FIFO, and tell the controller to send. */
	while (packet_size) {
		AT91C_UDP_FDR[endpoint] = *ptr;
		ptr++;
		packet_size--;
	}
	usb_csr_set_flag (endpoint, AT91C_UDP_TXPKTRDY);
}


/* Read one data packet from the USB controller.
 * Assume that usb_state.rx_data and usb_state.rx_len are set.
 */
static void usb_read_data (int endpoint)
{
	uint16_t i;
	uint16_t total;

	/* Given our configuration, we should only be getting packets on
	 * endpoint 1. Ignore data on any other endpoint.
	 * (note: data from EP0 are managed by usb_manage_setup())
	 */
	if (endpoint != 1) {
		usb_csr_clear_flag (
			endpoint, 
			AT91C_UDP_RX_DATA_BK0 | AT91C_UDP_RX_DATA_BK1
		);
		return;
	}

	/* must not happen ! */
	if (usb_state.rx_len > 0)
		return;

	total = (AT91C_UDP_CSR[endpoint] & AT91C_UDP_RXBYTECNT) >> 16;

	/* we start reading */
	/* all the bytes will be put in rx_data */
	for (i = 0; i < total && i < usb_state.rx_size; i++)
		usb_state.rx_data[i] = AT91C_UDP_FDR[1];

	usb_state.rx_len = i;

	/* if we have read all the byte ... */
	if (i == total) {
		/* Acknowledge reading the current RX bank, and switch to the other. */
		usb_csr_clear_flag (1, usb_state.current_rx_bank);
		if (usb_state.current_rx_bank == AT91C_UDP_RX_DATA_BK0)
			usb_state.current_rx_bank = AT91C_UDP_RX_DATA_BK1;
		else
			usb_state.current_rx_bank = AT91C_UDP_RX_DATA_BK0;
	}
	/* FIXME: check this ... */
	/* else we let the interruption running :
	 * after this function, the interruption should be disabled until
	 * a new buffer to read is provided */
}


/* On the endpoint 0: A stall is USB's way of sending
 * back an error (either "not understood" or "not handled
 * by this device"). The connexion will be reinitialized
 * by the host.
 * On the other endpoint : Indicates to the host that the endpoint is halted
 */
static void usb_send_stall (int endpoint)
{
	if (endpoint == 0)
		usb_state.status = USB_UNINITIALISED;
	usb_csr_set_flag (endpoint, AT91C_UDP_FORCESTALL);
}


/* During setup, we need to send packets with null data. */
static void usb_send_null (void)
{
	usb_write_data (0, NULL, 0);
}


/* Handle receiving and responding to setup packets on EP0. */
static uint32_t usb_manage_setup_packet (void)
{
	/* The structure of a USB setup packet. */
	struct {
		uint8_t request_attrs; /* Request characteristics. */
		uint8_t request; /* Request type. */
		uint16_t value; /* Request-specific value. */
		uint16_t index; /* Request-specific index. */
		uint16_t length; /* The number of bytes transferred in the (optional)
			     * second phase of the control transfer. */
	} packet;
	uint16_t response = 0;
	uint8_t byte_resp;

	/* Read the packet from the FIFO into the above packet struct. */
	packet.request_attrs = AT91C_UDP_FDR[0];
	packet.request       = AT91C_UDP_FDR[0];
	packet.value         = (AT91C_UDP_FDR[0] & 0xFF) | (AT91C_UDP_FDR[0] << 8);
	packet.index         = (AT91C_UDP_FDR[0] & 0xFF) | (AT91C_UDP_FDR[0] << 8);
	packet.length        = (AT91C_UDP_FDR[0] & 0xFF) | (AT91C_UDP_FDR[0] << 8);

	if ((packet.request_attrs & USB_BMREQUEST_DIR) == USB_BMREQUEST_D_TO_H) {
		usb_csr_set_flag (0, AT91C_UDP_DIR); /* TODO: contradicts atmel doc p475 */
	}

	usb_csr_clear_flag (0, AT91C_UDP_RXSETUP);

	/* Intercept class requests */
	if (usb_state.current_config == 1 
			&& (packet.request_attrs & (USB_BMREQUEST_CLASS | USB_BMREQUEST_RCPT))
				== (USB_BMREQUEST_CLASS | USB_BMREQUEST_RCPT_INT)
			&& (packet.index == 0)) {
		switch (packet.request) {
			case USB_BREQUEST_BULK_RESET:
				/* forward reset to MSD; */
				usb_send_null ();
				break;
			case USB_BREQUEST_GET_MAX_LUN:
				byte_resp = 1;
				usb_write_data (0, (uint8_t *)&byte_resp, 1);
				break;
			default:
				usb_send_stall (0);
				break;
		}
	}

	/* Respond to the control request. */
	switch (packet.request) {
		case USB_BREQUEST_GET_STATUS:
			/* The host wants to know our status.
			 *
			 * If it wants the device status, just reply that the NXT is still
			 * self-powered (as first declared by the setup packets). If it
			 * wants endpoint status, reply with that endpoints halt bit.
			 * Any other status request types are reserved, which
			 * translates to replying zero.
			 */
			switch (packet.request_attrs & USB_BMREQUEST_RCPT) {
				case USB_BMREQUEST_RCPT_DEV:
					response = 1; /* self powered */
					break;
				case USB_BMREQUEST_RCPT_EPT:
					{
						int ep = packet.index & 0x7f;
						
						if (ep < 8) {
							response = (usb_state.halted >> ep) & 1;
						} else {
							response = 0;
						}
					}
					break;
				default:
					response = 0;
					break;
			}

			usb_write_data (0, (uint8_t *)&response, 2);
			break;

		case USB_BREQUEST_SET_DESCRIPTOR:
			/* No descriptors that can be updated, so return Request Error. */
			usb_send_stall (0);
			break;

		case USB_BREQUEST_CLEAR_FEATURE:
		case USB_BREQUEST_SET_FEATURE:
			/* No features that can be set/cleared, so return Request Error. */
			if (((packet.request_attrs & USB_BMREQUEST_RCPT) == USB_BMREQUEST_RCPT_EPT)
					&& (packet.value == USB_FEAT_ENDPOINT_HALT)) {
				uint8_t ep = packet.index & 0x7f;
				
				if (ep < 4) {
					if (packet.value == USB_BREQUEST_SET_FEATURE) {
						usb_state.halted |= 1 << ep;
						usb_send_stall (ep);
					} else {
						usb_state.halted &= ~(1 << ep);
						*AT91C_UDP_RSTEP = (1 << ep);
						*AT91C_UDP_RSTEP = 0;
					}
				} else {
					usb_send_stall (0);
				}
			} else {
				usb_send_stall (0);
			}
			break;

		case USB_BREQUEST_GET_INTERFACE:
			if (usb_state.current_config == 1 && packet.index == 0) {
				byte_resp = 0;
				usb_write_data (0, (uint8_t *)&byte_resp, 1);
			} else {
				usb_send_stall (0);
			}
			break;

		case USB_BREQUEST_SET_INTERFACE:
			/* No alternate settings available, so return Request Error. */
			usb_send_stall (0);
			break;

		case USB_BREQUEST_SET_ADDRESS:
			/* The host has given the NXT a new USB address. This address
			 * must be set AFTER sending the ack packet. Therefore, we just
			 * remember the new address, and the interrupt handler will set
			 * it when the transmission completes.
			 */
			usb_state.new_device_address = packet.value;
			usb_send_null ();

			/* If the address change is to 0, do it immediately.
			 *
			 * TODO: Why? And when does this happen?
			 */
			if (usb_state.new_device_address == 0) {
				*AT91C_UDP_FADDR = AT91C_UDP_FEN;
				*AT91C_UDP_GLBSTATE = 0;
			}
			break;

		case USB_BREQUEST_GET_DESCRIPTOR:
			/* The host requested a descriptor. */
			{
				uint32_t size;
				uint8_t index = (packet.value & USB_WVALUE_INDEX);
				switch ((packet.value & USB_WVALUE_TYPE) >> 8) {
					case USB_DESC_TYPE_DEVICE: /* Device descriptor */
						size = usb_device_descriptor[0];
						usb_write_data (0, usb_device_descriptor,
								MIN(size, packet.length));
						break;

					case USB_DESC_TYPE_CONFIG: /* Configuration descriptor */
						usb_write_data (0, usb_full_config,
								MIN(usb_full_config[2], packet.length));

						/* FIXME: Why? This is not specified in the USB specs. */
						if (usb_full_config[2] < packet.length)
							usb_send_null ();
						break;

					case USB_DESC_TYPE_STR: /* String or language info. */
						if ((packet.value & USB_WVALUE_INDEX) == 0) {
							usb_write_data (0, usb_string_desc,
									MIN(usb_string_desc[0], packet.length));
						} else if (index <= usb_string_count) {
							/* The host wants a specific string. */
							usb_write_data (0, usb_strings[index-1],
									MIN(usb_strings[index-1][0],
										packet.length));
						} else {
							/* Not a string we have. */
							usb_send_stall (0);
						}
						break;

					case USB_DESC_TYPE_DEVICE_QUALIFIER: /* Device qualifier descriptor. */
						size = usb_dev_qualifier_desc[0];
						usb_write_data (0, usb_dev_qualifier_desc,
								MIN(size, packet.length));
						break;

					default: /* Unknown descriptor, tell the host by stalling. */
						usb_send_stall (0);
				}
				break;
			}

		case USB_BREQUEST_GET_CONFIG:
			/* The host wants to know the ID of the current configuration. */
			usb_write_data (0, (uint8_t *)&(usb_state.current_config), 1);
			break;

		case USB_BREQUEST_SET_CONFIG:
			/* The host selected a new configuration. */
			if (packet.value == 0 || packet.value == 1) {
				uint32_t csr1 = 0, csr2 = 0, csr3 = 0;

				/* we ack */
				usb_send_null ();

				usb_state.current_config = packet.value;
				usb_state.halted = 0x00;
				
				if (usb_state.current_config == 0) {
					*AT91C_UDP_GLBSTATE = AT91C_UDP_FADDEN;
					usb_state.status = USB_UNINITIALISED;
				} else {
					/* we set the register in configured mode */
					*AT91C_UDP_GLBSTATE = AT91C_UDP_CONFG | AT91C_UDP_FADDEN;
					csr1 = AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_BULK_OUT;
					csr2 = AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_BULK_IN;
					usb_state.status = USB_READY;
				}

				AT91C_UDP_CSR[1] = csr1;
				while (AT91C_UDP_CSR[1] != csr1);
				AT91C_UDP_CSR[2] = csr2;
				while (AT91C_UDP_CSR[2] != csr2);
				AT91C_UDP_CSR[3] = csr3;
				while (AT91C_UDP_CSR[3] != csr3);
			} else {
				usb_send_stall (0);
			}
			break;
		
		default:
			usb_send_stall (0);
			break;
	}

	return packet.request;
}


/* The main USB interrupt handler. */
static void usb_isr (void)
{
	uint8_t endpoint = 127;
	uint32_t csr, isr;

	isr = *AT91C_UDP_ISR;

	/* We sent a stall, the host has acknowledged the stall. */
	if (AT91C_UDP_CSR[0] & AT91C_UDP_ISOERROR)
		usb_csr_clear_flag (0, AT91C_UDP_FORCESTALL | AT91C_UDP_ISOERROR);

	/* End of bus reset. Starting the device setup procedure. */
	if (isr & AT91C_UDP_ENDBUSRES) {
		usb_state.status = USB_UNINITIALISED;

		/* Disable and clear all interruptions, reverting to the base
		 * state.
		 */
		*AT91C_UDP_IDR = ~0;
		*AT91C_UDP_ICR = ~0;

		/* Reset all endpoint FIFOs. */
		*AT91C_UDP_RSTEP = ~0;
		*AT91C_UDP_RSTEP = 0;

		/* Reset internal state. */
		usb_state.current_rx_bank = AT91C_UDP_RX_DATA_BK0;
		usb_state.current_config  = 0;

		/* Reset EP0 to a basic control endpoint. */
		/* TODO: The while is ugly. Fix it. */
		AT91C_UDP_CSR[0] = AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_CTRL;
		while (AT91C_UDP_CSR[0] != (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_CTRL));

		/* Enable interrupt handling for all three endpoints, as well as
		 * suspend/resume.
		 */
		*AT91C_UDP_IER = (AT91C_UDP_EPINT0 | AT91C_UDP_EPINT1 |
				AT91C_UDP_EPINT2 | AT91C_UDP_EPINT3 |
				AT91C_UDP_RXSUSP | AT91C_UDP_RXRSM);

		/* Enable the function endpoints, setting address 0, and return
		 * immediately. Given that we've just reset everything, there's no
		 * point in continuing.
		 */
		*AT91C_UDP_FADDR = AT91C_UDP_FEN;

		return;
	}

	if (isr & AT91C_UDP_WAKEUP) {
		*AT91C_UDP_ICR = AT91C_UDP_WAKEUP;
		isr &= ~AT91C_UDP_WAKEUP;
	}


	if (isr & AT91C_UDP_SOFINT) {
		*AT91C_UDP_ICR = AT91C_UDP_SOFINT;
		isr &= ~AT91C_UDP_SOFINT;
	}


	if (isr & AT91C_UDP_RXSUSP) {
		*AT91C_UDP_ICR = AT91C_UDP_RXSUSP;
		isr &= ~AT91C_UDP_RXSUSP;
		usb_state.pre_suspend_status = usb_state.status;
		usb_state.status = USB_SUSPENDED;
	}

	if (isr & AT91C_UDP_RXRSM) {
		*AT91C_UDP_ICR = AT91C_UDP_RXRSM;
		isr &= ~AT91C_UDP_RXRSM;
		usb_state.status = usb_state.pre_suspend_status;
	}

	for (endpoint = 0; endpoint < N_ENDPOINTS ; endpoint++) {
		if (isr & (1 << endpoint))
			break;
	}


	if (endpoint == 0) {
		if (AT91C_UDP_CSR[0] & AT91C_UDP_RXSETUP) {
			csr = usb_manage_setup_packet ();
			return;
		}
	}


	if (endpoint < N_ENDPOINTS) { /* if an endpoint was specified */
		csr = AT91C_UDP_CSR[endpoint];

		if (csr & AT91C_UDP_RX_DATA_BK0
				|| csr & AT91C_UDP_RX_DATA_BK1) {

			if (endpoint == 1) {
				AT91C_UDP_CSR[1] &= ~AT91C_UDP_EPEDS;
				while (AT91C_UDP_CSR[1] & AT91C_UDP_EPEDS);
			}

			usb_read_data (endpoint);

			if (endpoint == 1) {
				/* FIXME: plum in MSD */
			}

			return;
		}

		if (csr & AT91C_UDP_TXCOMP) {
			/* so first we will reset this flag */
			usb_csr_clear_flag (endpoint, AT91C_UDP_TXCOMP);

			if (usb_state.new_device_address > 0) {
				/* the previous message received was SET_ADDR */
				/* now that the computer ACK our send_null(), we can
				 * set this address for real */

				/* we set the specified usb address in the controller */
				*AT91C_UDP_FADDR    = AT91C_UDP_FEN | usb_state.new_device_address;
				/* and we tell the controller that we are in addressed mode now */
				*AT91C_UDP_GLBSTATE = AT91C_UDP_FADDEN;
				usb_state.new_device_address = 0;
			}

			/* and we will send the following data */
			if (usb_state.tx_len[endpoint] > 0
					&& usb_state.tx_data[endpoint] != NULL) {
				usb_write_data (endpoint, usb_state.tx_data[endpoint],
						usb_state.tx_len[endpoint]);
			} else {
				/* then it means that we sent all the data and the host has acknowledged it */
				usb_state.status = USB_READY;
			}
			
			if (usb_state.status == USB_READY && endpoint == 2) {
				/* FIXME: plum in MSD */
			}
			
			return;
		}

	}


	/* We clear also the unused bits,
	 * just "to be sure" */
	if (isr) {
		*AT91C_UDP_ICR = 0xFFFFC4F0;
	}
}


void usb_disable (void)
{
	aic_disable (AT91C_ID_UDP);

	*AT91C_PIOA_PER = (1 << 16);
	*AT91C_PIOA_OER = (1 << 16);
	*AT91C_PIOA_SODR = (1 << 16);

	systick_wait_ms (200);
}


static inline void usb_enable (void)
{
	/* Enable the UDP pull up by outputting a zero on PA.16 */
	/* Enabling the pull up will tell to the host (the computer) that
	 * we are ready for a communication
	 */
	*AT91C_PIOA_PER = (1 << 16);
	*AT91C_PIOA_OER = (1 << 16);
	*AT91C_PIOA_CODR = (1 << 16);

	systick_wait_ms (200);
}


void usb_init (void)
{
	usb_disable ();

	memset ((void*)&usb_state, 0, sizeof(usb_state));

	nxt__interrupts_disable ();

	/* usb pll was already set in init.S */

	/* enable peripheral clock */
	*AT91C_PMC_PCER = (1 << AT91C_ID_UDP);

	/* enable system clock */
	*AT91C_PMC_SCER = AT91C_PMC_UDP;

	/* disable all the interruptions */
	*AT91C_UDP_IDR = ~0;

	/* reset all the endpoints */
	*AT91C_UDP_RSTEP = 0xF;
	*AT91C_UDP_RSTEP = 0;

	*AT91C_UDP_ICR = 0xFFFFFFFF;

	/* Install the interruption routine */

	/* the first interruption we will get is an ENDBUSRES
	 * this interruption is always emit (can't be disable with UDP_IER)
	 */
	/* other interruptions will be enabled when needed */
	aic_install_isr (AT91C_ID_UDP, AIC_PRIO_DRIVER,
			AIC_TRIG_LEVEL, usb_isr);


	nxt__interrupts_enable ();

	usb_enable ();
}

#if 0

bool usb_can_write (void)
{
	  return (usb_state.status == USB_READY);
}


void usb_write (uint8_t *data, uint32_t length)
{
	/*
	   NX_ASSERT_MSG(usb_state.status != USB_UNINITIALISED,
	   "USB not init");
	   NX_ASSERT_MSG(usb_state.status != USB_SUSPENDED,
	   "USB asleep");
	   NX_ASSERT(data != NULL);
	   NX_ASSERT(length > 0);
	   */

	/* TODO: Make call asynchronous */
	while (usb_state.status != USB_READY);

	/* start sending the data */
	usb_write_data (2, data, length);
}

bool usb_data_written (void)
{
	return (usb_state.tx_len[2] == 0);
}


bool usb_is_connected (void)
{
	return (usb_state.status != USB_UNINITIALISED);
}


void usb_read (uint8_t *data, uint32_t length)
{
	usb_state.rx_data = data;
	usb_state.rx_size = length;
	usb_state.rx_len  = 0;

	if (usb_state.status > USB_UNINITIALISED
			&& usb_state.status != USB_SUSPENDED) {
		AT91C_UDP_CSR[1] |= AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_BULK_OUT;
	}
}


uint32_t usb_data_read (void)
{
	return usb_state.rx_len;
}

#endif
