// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_PROCESS_DIAGNOSTICS_H_
#define MOCKTAIL_RUNTIME_PROCESS_DIAGNOSTICS_H_

namespace mocktail::runtime {

enum class ProcessDiagnosticStage { kStartup, kNativeRuntime, kShutdown };

void InstallCpuLimitDiagnostics();
void LogProcessDiagnostics(ProcessDiagnosticStage stage);

}

#endif
