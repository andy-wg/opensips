/**
 *
 * drouting module callbacks
 *
 * Copyright (C) 2014-2106 OpenSIPS Solutions
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History
 * -------
 *  2014-09-13  initial version (Mihai Tiganus)
 *  2016-02-18  ported to 2.2 (bogdan)
 */


#include <stdlib.h>
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "dr_cb.h"

unsigned char sort_algs[N_MAX_SORT_CBS] = {0,'O','W','Q'};



#define POINTER_CLOSED_MARKER  ((void *)(-1))

unsigned char sort_algs[N_MAX_SORT_CBS] = {0,'O','W','Q'};

/* the array with all the cb lists (per type) */
static struct dr_callback *dr_cbs[DRCB_MAX];

/* an array of sorting cbs registered by different modules */
static struct dr_callback *dr_sort_cbs[N_MAX_SORT_CBS];



static void destroy_dr_callbacks_list(struct dr_callback *cb)
{
	struct dr_callback *cb_t;

	while(cb) {
		cb_t = cb;
		cb = cb->next;
		if(cb_t->callback_param_free && cb_t->param) {
			cb_t->callback_param_free(cb_t->param);
			cb_t->param = NULL;
		}
		shm_free(cb_t);
	}
}

void destroy_dr_cbs(int types) {
	int i;
	struct dr_callback *dr_sort_cb_it;

	if(types & DRCB_REG_ADD_RULE || types & DRCB_REG_CR
			|| types & DRCB_REG_GW || types & DRCB_REG_INIT_RULE) {
		if(dr_reg_cbs && dr_reg_cbs != POINTER_CLOSED_MARKER) {
			destroy_dr_callbacks_list(dr_reg_cbs->first);
			shm_free(dr_reg_cbs);
		}
		dr_reg_cbs = POINTER_CLOSED_MARKER;
	}
	else if(types & DRCB_ACC_CALL) {
		if(dr_acc_cbs && dr_acc_cbs != POINTER_CLOSED_MARKER) {
			destroy_dr_callbacks_list(dr_acc_cbs->first);
			shm_free(dr_acc_cbs);
		}
		dr_acc_cbs = POINTER_CLOSED_MARKER;
	}
	else if(types & DRCB_SORT_DST) {
		for(i=0; i <N_MAX_SORT_CBS; i++) {
			dr_sort_cb_it = dr_sort_cbs[i];
			if(dr_sort_cb_it->callback_param_free && dr_sort_cb_it->param) {
				dr_sort_cb_it->callback_param_free(dr_sort_cb_it->param);
				dr_sort_cb_it->param = NULL;
			}
		}
	}

}

/*
 * adds a given callback to a given callback list
 */
int insert_drcb(struct dr_head_cbl **dr_cb_list, struct dr_callback *cb,
		int types)
{
	int i;
	struct dr_callback *dr_sort_cb_it;

	for( i=0 ; i<DRCB_MAX ; i++ ) {
		if (dr_cbs[i] && dr_cbs[i]!=POINTER_CLOSED_MARKER)
			destroy_dr_callbacks_list(dr_cbs[i]);
		dr_cbs[i] = POINTER_CLOSED_MARKER;
	}

	for(i=0; i <N_MAX_SORT_CBS; i++) {
		if ( (dr_sort_cb_it=dr_sort_cbs[i])!=NULL &&
		dr_sort_cb_it->callback_param_free && dr_sort_cb_it->param) {
			dr_sort_cb_it->callback_param_free(dr_sort_cb_it->param);
			dr_sort_cb_it->param = NULL;
		}
	}
	cb->next = (*dr_cb_list)->first;
	(*dr_cb_list)->first = cb;
	(*dr_cb_list)->types |= types;
	return 0;
}

/* TODO: param will be the index in the array */
int register_dr_cb(int types, drouting_cb f, void *param,
		param_free_cb ff) {
	long int cb_sort_index = 0;

/* TODO: param will be the index in the array */
int register_dr_cb(enum drcb_types type, dr_cb f, void *param,
														dr_param_free_cb ff)
{
	long int cb_sort_index = 0;
	struct dr_callback *cb;

	if((types != 0) && (types & (types-1)) == 0) { /* check that the type is a
													  power of two - only one
													  type of cb selected */
		cb = (struct dr_callback*)shm_malloc(sizeof(struct dr_callback));
		if(cb == 0) {
			LM_ERR("no more shm memory\n");
			return -1;
		}

		cb->types = types;
		cb->callback = f;
		if(!(types & DRCB_SORT_DST)) {
			cb->param = param; /* because now param holds the type of the
								  sorting function */
		} else {
			cb->param = NULL;
		}
		cb->callback_param_free = ff;
		cb->next = NULL;

		if(types & DRCB_SORT_DST) {
			if(param == NULL) {
				LM_ERR("no index supplied for sort callback registered at dr\n");
				return -1;
			}
			cb_sort_index = (long int)param;
			if(cb_sort_index > N_MAX_SORT_CBS) {
				LM_ERR("Sort cbs array not large enough to accomodate cb at dr\n");
				return -1;
			}
			if(dr_sort_cbs[cb_sort_index] != NULL) {
				LM_WARN("[dr] sort callback at index '%ld' will be overwritten\n",
						cb_sort_index);
			}
			dr_sort_cbs[cb_sort_index] = cb;
		} else if(types & DRCB_ACC_CALL) {
			if(insert_drcb(&dr_acc_cbs, cb, types) == -1)
				goto error;
		} else if(types & (DRCB_REG_INIT_RULE | DRCB_REG_GW |
					DRCB_REG_CR | DRCB_REG_ADD_RULE
					| DRCB_REG_MARK_AS_RULE_LIST | DRCB_REG_LINK_LISTS
					| DRCB_REG_FREE_LIST)) {
			if(insert_drcb(&dr_reg_cbs, cb, types) == -1)
				goto error;
		} else if(types & (DRCB_SET_PROFILE)) {
			if(insert_drcb(&dr_set_profile_cbs, cb, types) == -1)
				goto error;
		}

	cb->callback = f;
	cb->callback_param_free = ff;
	cb->next = NULL;

	if (type!=DRCB_SORT_DST) {
		cb->param = param; /* because now param holds the type of the
							* sorting function */
		/* insert callback to the right list (based on type) */
		if( dr_cbs[type]==POINTER_CLOSED_MARKER) {
			LM_CRIT("DRCB_SORT_DST registered after shut down!\n");
			goto error;
		}
		cb->next = dr_cbs[type];
		dr_cbs[type] = cb;
	} else {
		cb->param = NULL;
		if(param == NULL) {
			LM_ERR("no index supplied for sort callback registered at dr\n");
			goto error;
		}
		cb_sort_index = (long int)param;
		if(cb_sort_index >= N_MAX_SORT_CBS) {
			LM_ERR("Sort cbs array not large enough to accommodate cb at dr\n");
			goto error;
		}
		if(dr_sort_cbs[cb_sort_index] != NULL) {
			LM_WARN("[dr] sort callback at index '%ld' will be overwritten\n",
				cb_sort_index);
		}
		dr_sort_cbs[cb_sort_index] = cb;
	}

	return 0;
error:
	shm_free(cb);
	return -1;
}


/* runs a callback from an array - sort_cb_type will represent
 * the index within the array */
int run_dr_sort_cbs(sort_cb_type type, void *param)
{
	if(dr_sort_cbs[type] == NULL) {
		LM_WARN("callback type '%d' not registered\n", type);
		return -1;
	}
	dr_sort_cbs[type]->callback(param);
	return 0;
}

/* runs a callback from an array - sort_cb_type will represent
 * the index within the array */
int run_indexed_callback(struct dr_callback **dr_sort_cbs_lst, sort_cb_type type,
		void *params, int max_type) {
	struct dr_cb_params *param = NULL;

	if(type > max_type) {
		LM_WARN("No such '%d' callback exists\n", type);
		return -1;
	}

	if(dr_sort_cbs_lst[type] == NULL) {
		LM_WARN("callback type '%d' not registered\n", type);
		return -1;
	}
	if(params != NULL) {
		param = (struct dr_cb_params *) pkg_malloc(sizeof(struct dr_cb_params));
		if(param == NULL) {
			LM_ERR("no more pkg memory\n");
			return -1;
		}
	}
	param->param = &params;
	dr_sort_cbs_lst[type]->callback(0, param);
	return 0;
}

int run_dr_cbs(struct dr_head_cbl *dr_cb_lst, int type, void *params) {
	struct dr_callback * dr_cb_it;
	struct dr_cb_params *param = NULL;

	param = (struct dr_cb_params *) shm_malloc(sizeof(struct dr_cb_params));
	if(param == NULL) {
		LM_ERR("no more shm memory\n");
		return -1;
	}

	if(dr_cb_lst != NULL && (dr_cb_lst->types & type)) {
		dr_cb_it = dr_cb_lst->first;
		while(dr_cb_it) {
			if(dr_cb_it->types & type) {
				if(params != NULL) {
					param->param = &params;
				} else {
					param->param = NULL;
				}
				dr_cb_it->callback(dr_cb_it->types, param);
			}
			dr_cb_it = dr_cb_it->next;
		}
	} else {
		LM_WARN("No callback list to match the given type\n");
		return -1;
	}
	return 0;
}

