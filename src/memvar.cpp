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

memvarBase::capacityType memvarBase::getHistoryCapacity() const noexcept
{
  return historyCapacity_;
}
}  // namespace memvar
