#include "psu.h"
#include "dongle.h"

float convert_byte_float(unsigned char * data) {

    int p1 = (data[1] >> 3) & 31;
    if (p1 > 15) p1 -= 32;

    int p2 = ((int)data[1] & 7) * 256 + (int)data[0];
    if (p2 > 1024) p2 = -(65536 - (p2 | 63488));

    return (float) p2 * powf(2.0, (float) p1);
}

void convert_float_byte(float val, int exp, unsigned char *data) {
    int p1;
    if (val > 0.0) {
        p1 = round(val * pow(2.0, (double) exp));
        if (p1 > 1023) p1 = 1023;
    } else {
        int p2 = round(val * pow(2.0, (double) exp));
        if (p2 < -1023) p2 = -1023;
        p1 = p2 & 2047;
    }
    data[0] = (unsigned char) (p1 & 0xff);
    exp = exp <= 0 ? -exp : 256 - exp;
    exp = exp << 3 & 255;
    data[1] = (unsigned char) (((p1 >> 8) & 0xff) | exp);
}

int send_init(int fd) {
    int s = 16;
    char d[16] = {0x54, 0x56, 0x56, 0x59, 0x55, 0x65, 0x69, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x00};
    char *data = d;
//    printf("INIT DATA TO WRITE:\n");
//    dump(data, s);
    int ret = xwrite(fd, data, s);

    return (ret != s) ? -1 : 0;
}

unsigned char * read_data_psu(int fd, int reg, int len) {
    unsigned char d[7] = {19, 3, 6, 1, 7, (char) len, (char) reg};
    unsigned char d2[3] = {8, 7, (char) len};

    if (data_write_dongle(fd, d, 7) != 0) return NULL;

    unsigned char * ret = data_read_dongle(fd, 1, NULL);

    if (!ret) return NULL;
    free(ret);

    if (data_write_dongle(fd, d2, 3) != 0) return NULL;

    ret = data_read_dongle(fd, len + 1, NULL);

//    printf("Read data psu:\n");
//    dump(ret, len + 1);

    return ret;
}

unsigned char * write_data_psu(int fd, int reg, unsigned char * data, int len) {
    unsigned char * d = (unsigned char *) malloc(len + 5);
    if (!d) return NULL;
    d[0] = 19;
    d[1] = 1;
    d[2] = 4;
    d[3] = len + 1;
    d[4] = reg;
    memcpy(d + 5, data, len);

    if (data_write_dongle(fd, d, len + 5) != 0) return NULL;

    unsigned char * ret = data_read_dongle(fd, 1, NULL);

//    printf("Write data psu:\n");
//    dump(ret, 2);

    return ret;
}

int set_page(int fd, int main, int page) {
    int r = -1;
    unsigned char c = (unsigned char) page;
    unsigned char * ret = write_data_psu(fd, (main ? 0 : 0xe7), &c, 1);
    unsigned char * ret2 = read_data_psu(fd, (main ? 0 : 0xe7), 1);

    if (!ret || !ret2 || ret2[0] != c) {
        printf("set_page (%s): set failed: %x, %x\n", (main ? "main" : "12v"), c, (ret2 ? ret2[0] : 0));
    } else if (ret && ret2) r = 0;

    if (ret) free(ret);
    if (ret2) free(ret2);

    return r;
}

int set_12v_page(int fd, int page) {
    return set_page(fd, 0, page);
}

int set_main_page(int fd, int page) {
    return set_page(fd, 1, page);
}

int read_psu_main_power(int fd) {
    unsigned char *ret;
    float unk1;

    if (set_main_page(fd, 0) == -1) return -1;

    if ((ret = read_data_psu(fd, 0x97, 2)) == NULL) return -1;
    unk1 = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0x89, 2)) == NULL) return -1;
    _psumain.current = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0x88, 2)) == NULL) return -1;
    _psumain.voltage = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0xee, 2)) == NULL) return -1;
    _psumain.outputpower = convert_byte_float(ret);
    free(ret);

    if (_psu_type == TYPE_AX1500) {
	if ((ret = read_data_psu(fd, 0xf2, 1)) == NULL) return -1;
	_psumain.cabletype = ret[0];
	free(ret);
    }

    _psumain.inputpower = (unk1 + (_psumain.voltage * _psumain.current))/2.;

    switch (_psu_type) {
	case TYPE_AX1500:
	    if (_psumain.voltage < 170.0) {
		if (_psumain.inputpower < 259.0) {
		    _psumain.outputpower = 0.9151 * _psumain.inputpower - 8.5209;
		    break;
		} else break;
	    } else if (_psumain.inputpower < 254.0) {
		_psumain.outputpower = 0.9394 * _psumain.inputpower - 62.289;
		break;
	    }
	break;
	case TYPE_AX1200:
	    if (_psumain.voltage < 170.0) {
		if (_psumain.inputpower < 201.0) {
		    _psumain.outputpower = 0.950565 * _psumain.inputpower - 11.98481;
		    break;
		} else break;
	    } else if (_psumain.inputpower < 195.0) {
		_psumain.outputpower = 0.97254 * _psumain.inputpower - 12.93532;
		break;
	    }
	break;
	case TYPE_AX860:
	    if (_psumain.voltage < 170.0) {
		if (_psumain.inputpower < 144.0) {
		    _psumain.outputpower = 0.958796 * _psumain.inputpower - 10.80566;
		    break;
		} else break;
	    } else if (_psumain.inputpower < 141.0) {
		_psumain.outputpower = 0.969644 * _psumain.inputpower - 10.59645;
		break;
	    }
	break;
	default: // AX760i
	    if (_psumain.voltage < 170.0) {
		if (_psumain.inputpower < 126.0) {
		    _psumain.outputpower = 0.958395 * _psumain.inputpower - 10.71166;
		    break;
		} else break;
	    } else if (_psumain.inputpower < 123.0) {
		_psumain.outputpower = 0.973022 * _psumain.inputpower - 10.8746;
		break;
	    }
	break;
    }

    if (_psumain.outputpower > _psumain.inputpower * 0.99) _psumain.outputpower = _psumain.inputpower * 0.99;

    _psumain.efficiency = (_psumain.outputpower/_psumain.inputpower) * 100.;

    return 0;
}

int read_psu_rail12v(int fd) {
    int i;
    unsigned char * ret;
    float f;
    int chnnum = (_psu_type == TYPE_AX1500 ? 10 : ((_psu_type == TYPE_AX1200) ? 8 : 6));
    for (i = 0; i < chnnum + 2; i++) {
//	printf("chnnum=%d\n", i);
	if (set_main_page(fd, 0) == -1) return -1;
	if (set_12v_page(fd, (_psu_type != TYPE_AX1200 && _psu_type != TYPE_AX1500 && i >= chnnum) ? i + 2 : i) == -1) return -1;

	if ((ret = read_data_psu(fd, 0x8b, 2)) == NULL) return -1;
	    if (i == chnnum) _rail12v.atx.voltage = convert_byte_float(ret);
	    else if (i == chnnum + 1) _rail12v.peripheral.voltage = convert_byte_float(ret);
	    else _rail12v.pcie[i].voltage = convert_byte_float(ret);
	free(ret);
	if ((ret = read_data_psu(fd, 0xe8, 2)) == NULL) return -1;
	    if (i == chnnum) _rail12v.atx.current = convert_byte_float(ret);
	    else if (i == chnnum + 1) _rail12v.peripheral.current = convert_byte_float(ret);
	    else _rail12v.pcie[i].current = convert_byte_float(ret);
	free(ret);
	if ((ret = read_data_psu(fd, 0xe9, 2)) == NULL) return -1;
	    if (i == chnnum) _rail12v.atx.power = convert_byte_float(ret);
	    else if (i == chnnum + 1) _rail12v.peripheral.power = convert_byte_float(ret);
	    else _rail12v.pcie[i].power = convert_byte_float(ret);
	free(ret);
	if ((ret = read_data_psu(fd, 0xea, 2)) == NULL) return -1;
	if (ret[0] == 0xff || (f = convert_byte_float(ret)) > 40.0) {
	    f = 40.0;
	    if (i == chnnum) {
		_rail12v.atx.ocp_enabled = false;
		_rail12v.atx.ocp_limit = f;
	    }else if (i == chnnum + 1) {
		_rail12v.peripheral.ocp_enabled = false;
		_rail12v.peripheral.ocp_limit = f;
	    } else {
		_rail12v.pcie[i].ocp_enabled = false;
		_rail12v.pcie[i].ocp_limit = f;
	    }
	} else {
	    if (f < 0.0) f = 0.0;
	    if (i == chnnum) {
		_rail12v.atx.ocp_enabled = true;
		_rail12v.atx.ocp_limit = f;
	    }else if (i == chnnum + 1) {
		_rail12v.peripheral.ocp_enabled = true;
		_rail12v.peripheral.ocp_limit = f;
	    } else {
		_rail12v.pcie[i].ocp_enabled = true;
		_rail12v.pcie[i].ocp_limit = f;
	    }
	}
	free(ret);
    }
    return 0;
}

int read_psu_railmisc(int fd) {
    unsigned char * ret;
    float unk1;

    if (set_main_page(fd, 1) == -1) return -1;

    if ((ret = read_data_psu(fd, 0x96, 2)) == NULL) return -1;
    unk1 = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0x8b, 2)) == NULL) return -1;
    _railmisc.rail_5v.voltage = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0x8c, 2)) == NULL) return -1;
    _railmisc.rail_5v.current = convert_byte_float(ret);
    free(ret);

    _railmisc.rail_5v.power = (unk1 + (_railmisc.rail_5v.voltage * _railmisc.rail_5v.current))/2.0;


    if (set_main_page(fd, 2) == -1) return -1;

    if ((ret = read_data_psu(fd, 0x96, 2)) == NULL) return -1;
    unk1 = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0x8b, 2)) == NULL) return -1;
    _railmisc.rail_3_3v.voltage = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0x8c, 2)) == NULL) return -1;
    _railmisc.rail_3_3v.current = convert_byte_float(ret);
    free(ret);

    _railmisc.rail_3_3v.power = (unk1 + (_railmisc.rail_3_3v.voltage * _railmisc.rail_3_3v.current))/2.0;

    return 0;
}

int set_psu_fan_fixed_percent(int fd, float f) {
    unsigned char percent = f;
    unsigned char * ret = write_data_psu(fd, 0x3b, &percent, 1);
    if (!ret) return -1;
    return 0;
}

int read_psu_fan_fixed_percent(int fd, int * i) {
    unsigned char * ret = read_data_psu(fd, 0x3b, 1);
    if (!ret) return -1;
    *i = (unsigned char)*ret;
    free(ret);
    return 0;
}

int set_psu_fan_mode(int fd, int m) {
    unsigned char mode = m;
    unsigned char * ret = write_data_psu(fd, 0xf0, &mode, 1);
    if (!ret) return -1;
    free(ret);
    return 0;
}

int read_psu_fan_mode(int fd, int * m) {
    unsigned char * ret = read_data_psu(fd, 0xf0, 1);
    if (!ret) return -1;
    *m = (unsigned char)*ret;
    free(ret);
    return 0;
}

int read_psu_fan_speed(int fd, float * f) {
    unsigned char * ret = read_data_psu(fd, 0x90, 2);
    if (!ret) return -1;
    *f = convert_byte_float(ret);
    free(ret);
    return 0;
}

int read_psu_temp(int fd, float * f) {
    unsigned char * ret = read_data_psu(fd, 0x8e, 2);
    if (!ret) return -1;
    *f = convert_byte_float(ret);
    free(ret);
    return 0;
}

char * dump_psu_type(int type) {
    switch (type) {
	case TYPE_AX860: return "AX860i"; break;
	case TYPE_AX1200: return "AX1200i"; break;
	case TYPE_AX1500: return "AX1500i"; break;
	default: return "AX760i"; break;
    }
}
