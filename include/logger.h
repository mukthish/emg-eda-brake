#ifndef LOGGER_H
#define LOGGER_H

#include "dispatcher.h"

void Logger_Init(void);
void Logger_LogEvent(SystemEvent evt);

#endif /* LOGGER_H */
