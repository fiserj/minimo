#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int mnm_run(void (* setup)(void), void (* draw)(void), void (* cleanup)(void));

#ifdef __cplusplus
}
#endif
