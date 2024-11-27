#include "Pch.h"

#include "CpuOperations.h"



void CpuOperations::Or (Byte * pRegisterAffected, Byte operand)
{
    *pRegisterAffected |= operand;
}
