/*
 * gtetrinet-ng — crash handling. A crash prints what it can to stderr and exits with
 * 128+signal; the launcher (gtetrinet-ng.sh) turns that into an anonymous fault report.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef GTN_FAULT_H
#define GTN_FAULT_H

void gtn_fault_install(void);
/* A real fault that is not a crash (it is logged with a FAULT: prefix the launcher reports). */
void gtn_fault(const char *what, const char *detail);

#endif
