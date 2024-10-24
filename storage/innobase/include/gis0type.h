/*****************************************************************************

Copyright (c) 2014, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2018, 2020, MariaDB Corporation.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA

*****************************************************************************/

/******************************************************************//**
@file include gis0type.h
R-tree header file

Created 2013/03/27 Jimmy Yang
***********************************************************************/

#ifndef gis0type_h
#define gis0type_h

#include "buf0buf.h"
#include "data0type.h"
#include "data0types.h"
#include "dict0types.h"
#include "ut0vec.h"
#include "gis0geo.h"

#include <vector>
#include <forward_list>

/** Node Sequence Number. Only updated when page splits */
typedef uint32_t     node_seq_t;

/* RTree internal non-leaf Nodes to be searched, from root to leaf */
struct node_visit_t {
	uint32_t	page_no;	/*!< the page number */
	node_seq_t	seq_no;		/*!< the SSN (split sequence number */
	ulint		level;		/*!< the page's index level */
	uint32_t	child_no;	/*!< child page num if for parent
					recording */
	btr_pcur_t*	cursor;		/*!< cursor structure if we positioned
					FIXME: there is no need to use whole
					btr_pcur_t, just the position related
					members */
	double		mbr_inc;	/*!< whether this node needs to be
					enlarged for insertion */
};

typedef std::vector<node_visit_t, ut_allocator<node_visit_t> >	rtr_node_path_t;

typedef	struct rtr_rec {
		rec_t*	r_rec;		/*!< matched record */
		bool	locked;		/*!< whether the record locked */
} rtr_rec_t;

typedef std::vector<rtr_rec_t, ut_allocator<rtr_rec_t> >	rtr_rec_vector;

/* Structure for matched records on the leaf page */
typedef	struct matched_rec {
	buf_block_t*	block;		/*!< the shadow buffer block */
	ulint		used;		/*!< memory used */
	rtr_rec_vector*	matched_recs;	/*!< vector holding the matching rec */
	ib_mutex_t	rtr_match_mutex;/*!< mutex protect the match_recs
					vector */
	bool		valid;		/*!< whether result in matched_recs
					or this search is valid (page not
					dropped) */
	bool		locked;		/*!< whether these recs locked */
} matched_rec_t;

/* In memory representation of a minimum bounding rectangle */
typedef struct rtr_mbr {
	double	xmin;			/*!< minimum on x */
	double	xmax;			/*!< maximum on x */
	double	ymin;			/*!< minimum on y */
	double	ymax;			/*!< maximum on y */
} rtr_mbr_t;

/* Maximum index level for R-Tree, this is consistent with BTR_MAX_LEVELS */
#define RTR_MAX_LEVELS		100

/* Number of pages we latch at leaf level when there is possible Tree
modification (split, shrink), we always latch left, current
and right pages */
#define RTR_LEAF_LATCH_NUM	3

/** Vectors holding the matching internal pages/nodes and leaf records */
typedef	struct rtr_info{
	rtr_node_path_t*path;	/*!< vector holding matching pages */
	rtr_node_path_t*parent_path;
				/*!< vector holding parent pages during
				search */
	matched_rec_t*	matches;/*!< struct holding matching leaf records */
	ib_mutex_t	rtr_path_mutex;
				/*!< mutex protect the "path" vector */
	buf_block_t*	tree_blocks[RTR_MAX_LEVELS + RTR_LEAF_LATCH_NUM];
				/*!< tracking pages that would be locked
				at leaf level, for future free */
        ulint		tree_savepoints[RTR_MAX_LEVELS + RTR_LEAF_LATCH_NUM];
				/*!< savepoint used to release latches/blocks
				on each level and leaf level */
	rtr_mbr_t	mbr;	/*!< the search MBR */
	que_thr_t*      thr;	/*!< the search thread */
	btr_cur_t*	cursor;	/*!< cursor used for search */
	dict_index_t*	index;	/*!< index it is searching */
	bool		need_prdt_lock;
				/*!< whether we will need predicate lock
				the tree */
	bool		need_page_lock;
				/*!< whether we will need predicate page lock
				the tree */
	bool		allocated;/*!< whether this structure is allocate or
				on stack */
	bool		mbr_adj;/*!< whether mbr will need to be enlarged
				for an insertion operation */
	bool		fd_del;	/*!< found deleted row */
	const dtuple_t*	search_tuple;
				/*!< search tuple being used */
	page_cur_mode_t	search_mode;
				/*!< current search mode */
} rtr_info_t;

/* Tracking structure for all ongoing search for an index */
struct rtr_info_track_t {
	/** Active search info */
	std::forward_list<rtr_info_t*, ut_allocator<rtr_info_t*> > rtr_active;
	ib_mutex_t rtr_active_mutex;
						/*!< mutex to protect
						rtr_active */
};

/* This is to record the record movement between pages. Used for corresponding
lock movement */
typedef struct rtr_rec_move {
	rec_t*		old_rec;	/*!< record being moved in old page */
	rec_t*		new_rec;	/*!< new record location */
	bool		moved;		/*!< whether lock are moved too */
} rtr_rec_move_t;
#endif /*!< gis0rtree.h */
