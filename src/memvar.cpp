/* 
 * File:   memvar.cpp
 * Author: massimo
 *
 * Created on October 19, 2017, 4:50 PM
 */

#include "memvar.h"
////////////////////////////////////////////////////////////////////////////////
namespace memvar
{
memvarBase::memvarBase(const memvarBase::capacityType historyCapacity) noexcept
:
historyCapacity_ (historyCapacity)
{}
}  // namespace memvar
