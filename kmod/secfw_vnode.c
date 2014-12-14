/*-
 * Copyright (c) 2014 Shawn Webb <shawn.webb@hardenedbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/acl.h>
#include <sys/kernel.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/pax.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/stat.h>

#include <security/mac/mac_policy.h>

#include "secfw.h"

int
secfw_check_prison(secfw_rule_t *rule, struct prison *pr)
{
	size_t i;

	if (rule->sr_nprisons == 0)
		return 1;

	for (i=0; i < rule->sr_nprisons; i++)
		if (!strcmp(rule->sr_prisonnames[i], pr->pr_name))
			return 1;

	return 0;
}

int
secfw_vnode_check_exec(struct ucred *ucred, struct vnode *vp,
    struct label *vplabel, struct image_params *imgp,
    struct label *execlabel)
{
	secfw_rule_t *rule;
	struct vattr vap;
	size_t i;
	int err, flags=0;

	err = VOP_GETATTR(vp, &vap, ucred);
	if (err)
		return (err);

	secfw_rules_lock_read();

	for (rule = rules.rules; rule != NULL; rule = rule->sr_next) {
		if (bcmp(&(rule->sr_fsid),
		    &(vp->v_mount->mnt_stat.f_fsid),
		    sizeof(struct fsid)) == 0) {
			if (vap.va_fileid == rule->sr_inode) {
				if (secfw_check_prison(rule, ucred->cr_prison) == 0)
					continue;

				for (i=0; i < rule->sr_nfeatures; i++) {
					switch(rule->sr_features[i].type) {
					case aslr_enabled:
						flags |= PAX_NOTE_ASLR;
						break;
					case aslr_disabled:
						flags |= PAX_NOTE_NOASLR;
						break;
					case segvguard_enabled:
						flags |= PAX_NOTE_SEGVGUARD;
						break;
					case segvguard_disabled:
						flags |= PAX_NOTE_NOSEGVGUARD;
						break;
					default:
						break;
					}
				}

				break;
			}
		}
	}

	secfw_rules_unlock_read();

	err = pax_elf(imgp, flags);

	return (err);
}

int
secfw_vnode_check_unlink(struct ucred *ucred, struct vnode *dvp,
    struct label *dvplabel, struct vnode *vp, struct label *vplabel,
    struct componentname *cnp)
{
	return (0);
}
