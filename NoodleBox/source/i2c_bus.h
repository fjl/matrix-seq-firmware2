//////////////////////////////////////////////////////////////////////////////
// sixty four pixels 2020                                       CC-NC-BY-SA //
//                                //  //          //                        //
//   //////   /////   /////   //////  //   /////  //////   /////  //   //   //
//   //   // //   // //   // //   //  //  //   // //   // //   //  // //    //
//   //   // //   // //   // //   //  //  /////// //   // //   //   ///     //
//   //   // //   // //   // //   //  //  //      //   // //   //  // //    //
//   //   //  /////   /////   //////   //  /////  //////   /////  //   //   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// I2C BUS MANAGER
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
#ifndef I2C_BUS_H_
#define I2C_BUS_H_

// Forward declaration
void i2c_master_callback(I2C_Type *base, i2c_master_handle_t *handle, status_t status, void *userData);

///////////////////////////////////////////////////////////////////////////////////
//
// Driver for the I2C DAC
//
///////////////////////////////////////////////////////////////////////////////////
class CI2CDac{
	enum { SZ_DATA = 8 };
	volatile byte m_data[SZ_DATA];
	volatile enum { ST_INIT1, ST_INIT2, ST_IDLE, ST_PENDING } m_state;
	uint16_t m_dac[4];
public:
	///////////////////////////////////////////////////////////////////////////////
	CI2CDac() {
		m_state = ST_INIT1;
		memset(m_dac,0,sizeof(m_dac));
	}
	///////////////////////////////////////////////////////////////////////////////
	inline void set(byte which, uint16_t value) {
		if(m_dac[which] != value) {
			m_dac[which] = value;
			if(m_state == ST_IDLE) {
				m_state = ST_PENDING;
			}
		}
	}
	///////////////////////////////////////////////////////////////////////////////
	byte get_tx(i2c_master_transfer_t& xfer) {
		switch(m_state) {
		case ST_INIT1:
			m_data[0] = 0b10001111; // set each channel to use internal vref
			xfer.dataSize = 1;
			break;
		case ST_INIT2:
			m_data[0] = 0b11001111; // set x2 gain on each channel
			xfer.dataSize = 1;
			break;
		case ST_PENDING:
#ifdef NB_PROTOTYPE
			// prototype version
			m_data[0] = ((m_dac[3]>>8) & 0xF);
			m_data[1] = (byte)m_dac[3];
			m_data[2] = ((m_dac[2]>>8) & 0xF);
			m_data[3] = (byte)m_dac[2];
#else
			// release version
			m_data[0] = ((m_dac[2]>>8) & 0xF);
			m_data[1] = (byte)m_dac[2];
			m_data[2] = ((m_dac[3]>>8) & 0xF);
			m_data[3] = (byte)m_dac[3];
#endif
			m_data[4] = ((m_dac[1]>>8) & 0xF);
			m_data[5] = (byte)m_dac[1];
			m_data[6] = ((m_dac[0]>>8) & 0xF);
			m_data[7] = (byte)m_dac[0];
			xfer.dataSize = 8;
			m_state = ST_IDLE;
			break;
		case ST_IDLE:
		default:
			return 0;
		}

		// configure the transfer object
		xfer.flags = kI2C_TransferDefaultFlag;
		xfer.direction = kI2C_Write;
		xfer.slaveAddress = I2C_ADDR_DAC;
		xfer.subaddress = 0;
		xfer.subaddressSize = 0;
		xfer.data = (byte*)&m_data;
		return 1;
	}

	///////////////////////////////////////////////////////////////////////////////
	void handle_rx(status_t status) {
		switch(m_state) {
		case ST_INIT1:
			m_state = ST_INIT2;
			break;
		case ST_INIT2:
			m_state = ST_PENDING;
			break;
		case ST_IDLE:
		case ST_PENDING:
			break;
		}
	}
};
CI2CDac g_i2c_dac;

///////////////////////////////////////////////////////////////////////////////////
//
// Driver for the I2C EEPROM
// M24256 is a 32kB EEPROM with a 64 byte page size
//
// memory is divided into slots. Each slot is 2560 bytes (40 pages)
// allowing for a total of 12 slots in 32kB
//
///////////////////////////////////////////////////////////////////////////////////
class CI2CEeprom{
	enum {
		SZ_PAGE_SIZE = 64,
		SZ_DATA = PATCH_SLOT_SIZE,
		WRITE_DELAY = 10
	};
	volatile byte m_data[SZ_DATA];
	volatile enum {
		ST_IDLE,
		ST_READ,
		ST_READ_COMPLETE,
		ST_READ_ERROR,
		ST_WRITE,
		ST_WRITE_DELAY,
		ST_WRITE_ERROR
	} m_state;

	volatile int m_slot;
	volatile int m_address;
	volatile int m_bytes;
	volatile int m_index;
	volatile byte m_ms_lsb;
	volatile byte m_write_delay;
	volatile status_t m_status;
public:
	///////////////////////////////////////////////////////////////////////////////
	CI2CEeprom() {
		m_address = 0;
		m_bytes = 0;
		m_index = 0;
		m_ms_lsb = 0;
		m_status = -1;
	}
	status_t get_status() {
		return m_status;
	}
	byte *buf() {
		return (byte*)&m_data;
	}
	int buf_size() {
		return SZ_DATA;
	}
	byte buf_checksum(int len) {
		byte result = 0;
		for(int i=0; i<len; ++i) {
			result+=m_data[i];
		}
		return result;
	}
	byte is_busy() {
		return (m_state != ST_IDLE);
	}
	byte read(int slot, int size) {
		if(m_state != ST_IDLE) {
			return 0;
		}
		ASSERT(size <= SZ_DATA);
		m_status = -1;
		m_slot = slot;
		m_address = slot * PATCH_SLOT_SIZE;
		m_bytes = size;
		m_state = ST_READ;
		return 1;
	}
	byte write(int slot, int size) {
		if(m_state != ST_IDLE) {
			return 0;
		}
		ASSERT(size <= SZ_DATA);
		m_status = -1;
		m_slot = slot;
		m_address = slot * PATCH_SLOT_SIZE;
		m_bytes = size;
		m_index = 0;
		m_state = ST_WRITE;
		return 1;
	}
	byte get_tx(i2c_master_transfer_t& xfer) {
		xfer.slaveAddress = I2C_ADDR_EEPROM;
		xfer.subaddress = m_address;
		xfer.subaddressSize = 2;
		xfer.flags = kI2C_TransferDefaultFlag;
		switch(m_state) {
			case ST_READ:
				xfer.direction = kI2C_Read;
				xfer.data = (byte*)&m_data;
				xfer.dataSize = m_bytes;
			    return 1;
			case ST_WRITE_DELAY:
				if(m_write_delay) {
					if((byte)g_clock.get_ms() != m_ms_lsb) {
						m_ms_lsb = (byte)g_clock.get_ms();
						--m_write_delay;
					}
					return 0;
				}
				else {
					if(m_bytes <= 0) {
						// done
						fire_event(EV_SAVE_OK,m_slot);
						m_state = ST_IDLE;
						return 0;
					}
					else {
						// write the next page
						m_state = ST_WRITE;
					}
				}
				// no break
			case ST_WRITE:
				xfer.direction = kI2C_Write;
				xfer.data = (byte*)&m_data[m_index];
				xfer.dataSize = SZ_PAGE_SIZE;
				return 1;
			case ST_READ_COMPLETE:
				fire_event(EV_LOAD_OK,m_slot);
				m_state = ST_IDLE;
				break;
			case ST_READ_ERROR:
				fire_event(EV_LOAD_FAIL,m_slot);
				m_state = ST_IDLE;
				break;
			case ST_WRITE_ERROR:
				fire_event(EV_SAVE_FAIL,m_slot);
				m_state = ST_IDLE;
				break;
		}
		return 0;
	}

	///////////////////////////////////////////////////////////////////////////////
	void handle_rx(status_t status) {
		m_status = status;
		switch(m_state) {
		case ST_READ:
			m_state = ST_READ_COMPLETE;
			break;
		case ST_WRITE:
			if(status == kStatus_Success) {

				m_address += SZ_PAGE_SIZE;
				m_index += SZ_PAGE_SIZE;
				m_bytes -= SZ_PAGE_SIZE;

				// post-write delay allows EEPROM to commit the page
				m_ms_lsb = (byte)g_clock.get_ms();
				m_write_delay = WRITE_DELAY;
				m_state = ST_WRITE_DELAY;
			}
			else {
				// error
				m_state = ST_WRITE_ERROR;
			}
			break;
		}
	}
};
CI2CEeprom g_i2c_eeprom;



///////////////////////////////////////////////////////////////////////////////////
//
// I2C bus manager class
//
///////////////////////////////////////////////////////////////////////////////////
class CI2CBus{
private:
	i2c_master_handle_t m_handle;
	i2c_master_transfer_t m_xfer;
	volatile enum { ST_IDLE, ST_EEPROM_BUSY, ST_DAC_BUSY } m_state;
public:
	///////////////////////////////////////////////////////////////////////////////
	CI2CBus() {
		m_state = ST_IDLE;
	}
	///////////////////////////////////////////////////////////////////////////////
	byte is_busy() {
		return (m_state != ST_IDLE) || g_i2c_eeprom.is_busy();
	}
	///////////////////////////////////////////////////////////////////////////////
	void init() {
		i2c_master_config_t masterConfig;
		I2C_MasterGetDefaultConfig(&masterConfig);
		masterConfig.baudRate_Bps = 500000U;

#ifdef NB_PROTOTYPE
		I2C_MasterInit(I2C0, &masterConfig, CLOCK_GetFreq(kCLOCK_BusClk));
		I2C_Enable(I2C0, true);
		#define I2C_DAC	I2C0
		#define I2C_EEPROM	I2C0
#else
		I2C_MasterInit(I2C0, &masterConfig, CLOCK_GetFreq(kCLOCK_BusClk));
		I2C_Enable(I2C0, true);
		I2C_MasterInit(I2C1, &masterConfig, CLOCK_GetFreq(kCLOCK_BusClk));
		I2C_Enable(I2C1, true);
		#define I2C_DAC	I2C0
		#define I2C_EEPROM	I2C1
#endif

	}
	///////////////////////////////////////////////////////////////////////////////
	void run() {
		if(m_state == ST_IDLE) {
			if(g_i2c_eeprom.get_tx(m_xfer)) {
				I2C_MasterTransferCreateHandle(I2C_EEPROM, &m_handle, i2c_master_callback, NULL);
				I2C_MasterTransferNonBlocking(I2C_EEPROM, &m_handle, &m_xfer);
				m_state = ST_EEPROM_BUSY;
			}
			else if(g_i2c_dac.get_tx(m_xfer)) {
				I2C_MasterTransferCreateHandle(I2C_DAC, &m_handle, i2c_master_callback, NULL);
				I2C_MasterTransferNonBlocking(I2C_DAC, &m_handle, &m_xfer);

				m_state = ST_DAC_BUSY;
			}
		}
	}
	///////////////////////////////////////////////////////////////////////////////
	void wait_for_idle() {
		do {
			run();
		} while(is_busy());
	}
	///////////////////////////////////////////////////////////////////////////////
	inline void on_txn_complete(I2C_Type *base, i2c_master_handle_t *handle, status_t status, void *userData) {
		switch(m_state) {
		case ST_EEPROM_BUSY:
			g_i2c_eeprom.handle_rx(status);
			break;
		case ST_DAC_BUSY:
			g_i2c_dac.handle_rx(status);
			break;
		}
		m_state = ST_IDLE;
	}
};

CI2CBus g_i2c_bus;
void i2c_master_callback(I2C_Type *base, i2c_master_handle_t *handle, status_t status, void *userData)
{
	g_i2c_bus.on_txn_complete(base, handle, status, userData);
}

#endif /* I2C_BUS_H_ */
