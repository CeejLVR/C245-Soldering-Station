/* include/hooks.h */
#ifndef HOOKS_H
#define HOOKS_H

/* Declared here so FreeRTOSConfig.h's configASSERT has a prototype.
   The vApplication...Hook functions are called by the kernel, not by us. */
void vAssertCalled(const char *file, int line);

#endif