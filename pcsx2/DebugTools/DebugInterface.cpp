#include "PrecompiledHeader.h"

#include "DebugInterface.h"
#include "Memory.h"
#include "R5900.h"
#include "AppCoreThread.h"
#include "Debug.h"
#include "../VU.h"

#include "../R3000A.h"
#include "../IopMem.h"

extern AppCoreThread CoreThread;

R5900DebugInterface r5900Debug;
R3000DebugInterface r3000Debug;

enum { EECAT_GPR, EECAT_CP0, EECAT_CP1, EECAT_CP2F, EECAT_CP2I, EECAT_COUNT };
enum { IOPCAT_GPR, IOPCAT_COUNT };

//
// DebugInterface
//

bool DebugInterface::isAlive()
{
	return GetCoreThread().IsOpen();
}

bool DebugInterface::isCpuPaused()
{
	return GetCoreThread().IsPaused();
}

void DebugInterface::pauseCpu()
{
	SysCoreThread& core = GetCoreThread();
	if (!core.IsPaused())
		core.Pause();
}

void DebugInterface::resumeCpu()
{
	SysCoreThread& core = GetCoreThread();
	if (core.IsPaused())
		core.Resume();
}


//
// R5900DebugInterface
//

u32 R5900DebugInterface::read8(u32 address)
{
	if (!isValidAddress(address))
		return -1;
	return memRead8(address);
}

u32 R5900DebugInterface::read16(u32 address)
{
	if (!isValidAddress(address))
		return -1;
	return memRead16(address);
}

u32 R5900DebugInterface::read32(u32 address)
{
	if (!isValidAddress(address))
		return -1;
	return memRead32(address);
}

u64 R5900DebugInterface::read64(u32 address)
{
	if (!isValidAddress(address))
		return -1;

	u64 result;
	memRead64(address,result);
	return result;
}

u128 R5900DebugInterface::read128(u32 address)
{
	if (!isValidAddress(address))
		return u128::From32(-1);

	u128 result;
	memRead128(address,result);
	return result;
}

void R5900DebugInterface::write8(u32 address, u8 value)
{
	if (!isValidAddress(address))
		return;

	memWrite8(address,value);
}

int R5900DebugInterface::getRegisterCategoryCount()
{
	return EECAT_COUNT;
}

const char* R5900DebugInterface::getRegisterCategoryName(int cat)
{
	switch (cat)
	{
	case EECAT_GPR:
		return "GPR";
	case EECAT_CP0:
		return "CP0";
	case EECAT_CP1:
		return "CP1";
	case EECAT_CP2F:
		return "CP2f";
	case EECAT_CP2I:
		return "CP2i";
	default:
		return "Invalid";
	}
}

int R5900DebugInterface::getRegisterSize(int cat)
{
	switch (cat)
	{
	case EECAT_GPR:
	case EECAT_CP2F:
		return 128;
	case EECAT_CP0:
	case EECAT_CP1:
	case EECAT_CP2I:
		return 32;
	default:
		return 0;
	}
}

int R5900DebugInterface::getRegisterCount(int cat)
{
	switch (cat)
	{
	case EECAT_GPR:
		return 35;	// 32 + pc + hi + lo
	case EECAT_CP0:
	case EECAT_CP1:
	case EECAT_CP2F:
	case EECAT_CP2I:
		return 32;
	default:
		return 0;
	}
}

DebugInterface::RegisterType R5900DebugInterface::getRegisterType(int cat)
{
	switch (cat)
	{
	case EECAT_GPR:
	case EECAT_CP0:
	case EECAT_CP2F:
	case EECAT_CP2I:
	default:
		return NORMAL;
	case EECAT_CP1:
		return SPECIAL;
	}
}

const char* R5900DebugInterface::getRegisterName(int cat, int num)
{
	switch (cat)
	{
	case EECAT_GPR:
		switch (num)
		{
		case 32:	// pc
			return "pc";
		case 33:	// hi
			return "hi";
		case 34:	// lo
			return "lo";
		default:
			return R5900::disRNameGPR[num];
		}
	case EECAT_CP0:
		return R5900::disRNameCP0[num];
	case EECAT_CP1:
		return R5900::disRNameCP1[num];
	case EECAT_CP2F:
		return disRNameCP2f[num];
	case EECAT_CP2I:
		return disRNameCP2i[num];
	default:
		return "Invalid";
	}
}

u128 R5900DebugInterface::getRegister(int cat, int num)
{
	u128 result;
	switch (cat)
	{
	case EECAT_GPR:
		switch (num)
		{
		case 32:	// pc
			result = u128::From32(cpuRegs.pc);
			break;
		case 33:	// hi
			result = cpuRegs.HI.UQ;
			break;
		case 34:	// lo
			result = cpuRegs.LO.UQ;
			break;
		default:
			result = cpuRegs.GPR.r[num].UQ;
			break;
		}
		break;
	case EECAT_CP0:
		result = u128::From32(cpuRegs.CP0.r[num]);
		break;
	case EECAT_CP1:
		result = u128::From32(fpuRegs.fpr[num].UL);
		break;
	case EECAT_CP2F:
		result = VU1.VF[num].UQ;
		break;
	case EECAT_CP2I:
		result = u128::From32(VU1.VI[num].UL);
		break;
	default:
		result.From32(0);
		break;
	}

	return result;
}

wxString R5900DebugInterface::getRegisterString(int cat, int num)
{
	switch (cat)
	{
	case EECAT_GPR:
	case EECAT_CP0:
		return getRegister(cat,num).ToString();
	case EECAT_CP1:
		{
			char str[64];
			sprintf(str,"%f",fpuRegs.fpr[num].f);
			return wxString(str,wxConvUTF8);
		}
	default:
		return L"";
	}
}


u128 R5900DebugInterface::getHI()
{
	return cpuRegs.HI.UQ;
}

u128 R5900DebugInterface::getLO()
{
	return cpuRegs.LO.UQ;
}

u32 R5900DebugInterface::getPC()
{
	return cpuRegs.pc;
}

void R5900DebugInterface::setPc(u32 newPc)
{
	cpuRegs.pc = newPc;
}

std::string R5900DebugInterface::disasm(u32 address)
{
	std::string out;

	u32 op = read32(address);
	R5900::disR5900Fasm(out,op,address);
	return out;
}


//
// R3000DebugInterface
//


u32 R3000DebugInterface::read8(u32 address)
{
	if (!isValidAddress(address))
		return -1;
	return iopMemRead8(address);
}

u32 R3000DebugInterface::read16(u32 address)
{
	if (!isValidAddress(address))
		return -1;
	return iopMemRead16(address);
}

u32 R3000DebugInterface::read32(u32 address)
{
	if (!isValidAddress(address))
		return -1;
	return iopMemRead32(address);
}

u64 R3000DebugInterface::read64(u32 address)
{
	return 0;
}

u128 R3000DebugInterface::read128(u32 address)
{
	return u128::From32(0);
}

void R3000DebugInterface::write8(u32 address, u8 value)
{
	if (!isValidAddress(address))
		return;

	iopMemWrite8(address,value);
}

int R3000DebugInterface::getRegisterCategoryCount()
{
	return IOPCAT_COUNT;
}

const char* R3000DebugInterface::getRegisterCategoryName(int cat)
{
	switch (cat)
	{
	case IOPCAT_GPR:
		return "GPR";
	default:
		return "Invalid";
	}
}

int R3000DebugInterface::getRegisterSize(int cat)
{
	switch (cat)
	{
	case IOPCAT_GPR:
		return 32;
	default:
		return 0;
	}
}

int R3000DebugInterface::getRegisterCount(int cat)
{
	switch (cat)
	{
	case IOPCAT_GPR:
		return 35;	// 32 + pc + hi + lo
	default:
		return 0;
	}
}

DebugInterface::RegisterType R3000DebugInterface::getRegisterType(int cat)
{
	switch (cat)
	{
	case IOPCAT_GPR:
	default:
		return DebugInterface::NORMAL;
	}
}

const char* R3000DebugInterface::getRegisterName(int cat, int num)
{
	switch (cat)
	{
	case IOPCAT_GPR:
		switch (num)
		{
		case 32:	// pc
			return "pc";
		case 33:	// hi
			return "hi";
		case 34:	// lo
			return "lo";
		default:
			return R5900::disRNameGPR[num];
		}
	default:
		return "Invalid";
	}
}

u128 R3000DebugInterface::getRegister(int cat, int num)
{
	u32 value;
	
	switch (cat)
	{
	case IOPCAT_GPR:
		switch (num)
		{
		case 32:	// pc
			value = psxRegs.pc;
			break;
		case 33:	// hi
			value = psxRegs.GPR.n.hi;
			break;
		case 34:	// lo
			value = psxRegs.GPR.n.lo;
			break;
		default:
			value = psxRegs.GPR.r[num];
			break;
		}
		break;
	default:
		value = -1;
		break;
	}

	return u128::From32(value);
}

wxString R3000DebugInterface::getRegisterString(int cat, int num)
{
	switch (cat)
	{
	case IOPCAT_GPR:
		return getRegister(cat,num).ToString();
	default:
		return L"Invalid";
	}
}

u128 R3000DebugInterface::getHI()
{
	return u128::From32(psxRegs.GPR.n.hi);
}

u128 R3000DebugInterface::getLO()
{
	return u128::From32(psxRegs.GPR.n.lo);
}

u32 R3000DebugInterface::getPC()
{
	return psxRegs.pc;
}

void R3000DebugInterface::setPc(u32 newPc)
{
	psxRegs.pc = newPc;
}

std::string R3000DebugInterface::disasm(u32 address)
{
	std::string out;

	u32 op = read32(address);
	R5900::disR5900Fasm(out,op,address);
	return out;
}