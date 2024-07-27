#ifndef BML_BML_H
#define BML_BML_H

#include "BML/Defines.h"
#include "BML/IDataShare.h"
#include "BML/IEventPublisher.h"
#include "BML/IConfiguration.h"

BML_BEGIN_CDECLS

BML_EXPORT BML::IDataShare *BML_GetDataShare(const char *name);
BML_EXPORT BML::IEventPublisher *BML_GetEventPublisher(const char *name);
BML_EXPORT BML::IConfiguration *BML_GetConfiguration(const char *name);

BML_END_CDECLS

#endif // BML_BML_H