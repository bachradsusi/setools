/**
 *  @file
 *  Implementation for the public interface for searching and iterating over
 *  range transition rules.
 *
 *  @author Kevin Carr kcarr@tresys.com
 *  @author Jeremy A. Mowery jmowery@tresys.com
 *  @author Jason Tang jtang@tresys.com
 *
 *  Copyright (C) 2006-2007 Tresys Technology, LLC
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "iterator_internal.h"
#include <qpol/iterator.h>
#include <qpol/policy.h>
#include <qpol/mlsrule_query.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/avtab.h>
#include <sepol/policydb/util.h>
#include <stdlib.h>
#include "qpol_internal.h"

typedef struct range_trans_state
{
    unsigned int bucket;
    hashtab_ptr_t cur_item;
	range_trans_t *cur;
} range_trans_state_t;

static int range_trans_state_end(const qpol_iterator_t * iter)
{
	range_trans_state_t *rs = NULL;

	if (!iter || !(rs = qpol_iterator_state(iter))) {
		errno = EINVAL;
		return STATUS_ERR;
	}

	return rs->cur ? 0 : 1;
}

static void *range_trans_state_get_cur(const qpol_iterator_t * iter)
{
	range_trans_state_t *rs = NULL;
    const policydb_t *db = NULL;

	if (!iter || !(rs = qpol_iterator_state(iter)) || !(db = qpol_iterator_policy(iter))) {
		errno = EINVAL;
		return NULL;
	}

	return rs->cur;
}

static int range_trans_state_next(qpol_iterator_t * iter)
{
	range_trans_state_t *rs = NULL;
    const policydb_t *db = NULL;

	if (!iter || !(rs = qpol_iterator_state(iter))  || !(db = qpol_iterator_policy(iter))) {
		errno = EINVAL;
		return STATUS_ERR;
	}

	if (range_trans_state_end(iter)) {
		errno = EINVAL;
		return STATUS_ERR;
	}

    rs->cur_item = rs->cur_item->next;
    while (rs->cur_item == NULL) {
        rs->bucket++;
        if (rs->bucket >= db->range_tr->size) {
            break;
        }

        rs->cur_item = db->range_tr->htable[rs->bucket];
    }

    if (rs->cur_item == NULL) {
        rs->cur = NULL;
    } else {
        rs->cur = (range_trans_t*)rs->cur_item->key;
    }

	return STATUS_SUCCESS;
}

static size_t range_trans_state_size(const qpol_iterator_t * iter)
{
	range_trans_state_t *rs = NULL;
    const policydb_t *db = NULL;
	size_t count = 0;
    unsigned int i = 0;

	if (!iter || !(rs = qpol_iterator_state(iter)) || !(db = qpol_iterator_policy(iter))) {
		errno = EINVAL;
		return 0;
	}

    hashtab_ptr_t cur = NULL;
    for (i = 0; i < db->range_tr->size; i++) {
        cur = db->range_tr->htable[i];
        while (cur != NULL) {
            count++;
            cur = cur->next;
        }
    }

	return count;
}

int qpol_policy_get_range_trans_iter(const qpol_policy_t * policy, qpol_iterator_t ** iter)
{
	policydb_t *db = NULL;
	range_trans_state_t *rs = NULL;
	int error = 0;

	if (iter)
		*iter = NULL;

	if (!policy || !iter) {
		ERR(policy, "%s", strerror(EINVAL));
		errno = EINVAL;
		return STATUS_ERR;
	}

	db = &policy->p->p;

	rs = calloc(1, sizeof(range_trans_state_t));
	if (!rs) {
		error = errno;
		ERR(policy, "%s", strerror(error));
		errno = error;
		return STATUS_ERR;
	}

	if (qpol_iterator_create(policy, (void *)rs, range_trans_state_get_cur,
				 range_trans_state_next, range_trans_state_end, range_trans_state_size, free, iter)) {
		error = errno;
		free(rs);
		errno = error;
		return STATUS_ERR;
	}

    rs->bucket = 0;
    rs->cur_item = db->range_tr->htable[0];
    rs->cur = NULL;

    rs->cur_item = db->range_tr->htable[rs->bucket];
    while (rs->cur_item == NULL) {
        rs->bucket++;
        if (rs->bucket >= db->range_tr->size) {
            break;
        }

        rs->cur_item = db->range_tr->htable[rs->bucket];
    }

    if (rs->cur_item != NULL) {
        rs->cur = (range_trans_t*)rs->cur_item->key;
    }
    
	return STATUS_SUCCESS;
}

int qpol_range_trans_get_source_type(const qpol_policy_t * policy, const qpol_range_trans_t * rule, const qpol_type_t ** source)
{
	policydb_t *db = NULL;
	range_trans_t *rt = NULL;

	if (source) {
		*source = NULL;
	}

	if (!policy || !rule || !source) {
		errno = EINVAL;
		ERR(policy, "%s", strerror(EINVAL));
		return STATUS_ERR;
	}

	db = &policy->p->p;
	rt = (range_trans_t *) rule;

	*source = (qpol_type_t *) db->type_val_to_struct[rt->source_type - 1];

	return STATUS_SUCCESS;
}

int qpol_range_trans_get_target_type(const qpol_policy_t * policy, const qpol_range_trans_t * rule, const qpol_type_t ** target)
{
	policydb_t *db = NULL;
	range_trans_t *rt = NULL;

	if (target) {
		*target = NULL;
	}

	if (!policy || !rule || !target) {
		ERR(policy, "%s", strerror(EINVAL));
		errno = EINVAL;
		return STATUS_ERR;
	}

	db = &policy->p->p;
	rt = (range_trans_t *) rule;

	*target = (qpol_type_t *) db->type_val_to_struct[rt->target_type - 1];

	return STATUS_SUCCESS;
}

int qpol_range_trans_get_target_class(const qpol_policy_t * policy, const qpol_range_trans_t * rule, const qpol_class_t ** target)
{
	policydb_t *db = NULL;
	range_trans_t *rt = NULL;

	if (target) {
		*target = NULL;
	}

	if (!policy || !rule || !target) {
		ERR(policy, "%s", strerror(EINVAL));
		errno = EINVAL;
		return STATUS_ERR;
	}

	db = &policy->p->p;
	rt = (range_trans_t *) rule;

	*target = (qpol_class_t *) db->class_val_to_struct[rt->target_class - 1];

	return STATUS_SUCCESS;
}

int qpol_range_trans_get_range(const qpol_policy_t * policy, const qpol_range_trans_t * rule, const qpol_mls_range_t ** range)
{
	range_trans_t *rt = NULL;

	if (range) {
		*range = NULL;
	}

	if (!policy || !rule || !range) {
		ERR(policy, "%s", strerror(EINVAL));
		errno = EINVAL;
		return STATUS_ERR;
	}

    policydb_t *db = &policy->p->p;
	rt = (range_trans_t *) rule;
    mls_range_t *target_range = NULL;

    target_range = hashtab_search(db->range_tr, (hashtab_key_t)rt);
    if (target_range == NULL) {
        return STATUS_ERR;
    }

	*range = (qpol_mls_range_t *)target_range;

	return STATUS_SUCCESS;
}
