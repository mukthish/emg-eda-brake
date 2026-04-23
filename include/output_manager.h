#ifndef OUTPUT_MGR_H
#define OUTPUT_MGR_H

#include <stdint.h>

// Expose initialization and the main control function
void OutputMgr_Init(void);
void OutputMgr_SetBrakes(uint8_t brake_state);
void OutputMgr_SetCalibrationLED(uint8_t state);

#endif /* OUTPUT_MGR_H */
