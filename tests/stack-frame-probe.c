/* SPDX-License-Identifier: GPL-3.0-only
 * Negative compile test only. Never link into a firmware or run on hardware.
 * The native compiler must reject this frame with the production frame limit. */
extern void stack_probe_consume(volatile unsigned char *,unsigned);
int stack_frame_probe(void){
 volatile unsigned char bytes[9000];
 stack_probe_consume(bytes,sizeof bytes);
 return bytes[0];
}
