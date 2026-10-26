#pragma once

// FreeRTOS tasks of the survey application. Data flow:
//
//   gnssTask ──► SharedState ◄── sensorTask
//                    │
//              sampleTask (fixed period, seq#, timestamp, UTM)
//                 │                │
//             logQueue          netQueue
//                 │                │
//             logTask           netTask
//   uiTask: button -> observation marks, LEDs     monitorTask: periodic health report
//   consoleTask: serial commands (Wi-Fi settings)

namespace app {

void gnssTask(void*);
void sensorTask(void*);
void sampleTask(void*);
void logTask(void*);
void netTask(void*);
void uiTask(void*);
void monitorTask(void*);
void consoleTask(void*);

}  // namespace app
