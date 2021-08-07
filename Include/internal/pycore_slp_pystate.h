/*
 * Copyright 2018 Anselm Kruis <anselm.kruis@atos.net>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/* This include file is included from pycore_pystate.h only */

/* how many cstacks to cache at all */
#ifndef SLP_CSTACK_MAXCACHE
#define SLP_CSTACK_MAXCACHE     100
#endif

/* adjust slots to typical size of a few recursions on your system */
#ifndef SLP_CSTACK_SLOTS
#define SLP_CSTACK_SLOTS        1024
#endif

typedef struct {
    struct _slp_cstack * cstack_chain;          /* the chain of all C-stacks of this interpreter. This is an uncounted/borrowed ref. */
    PyObject * reduce_frame_func;               /* a function used to pickle frames */
    PyObject * error_handler;                   /* the Stackless error handler */
    PyObject * channel_hook;                    /* the channel callback function */
    struct _slp_bomb * mem_bomb;                /* a permanent bomb to use for memory errors */
    PyObject * schedule_hook;                   /* the schedule callback function */
    slp_schedule_hook_func * schedule_fasthook; /* the fast C-only schedule_hook */
    struct _ts * initial_tstate;                /* recording the main thread state */
    uint8_t enable_softswitch;                  /* the flag which decides whether we try to use soft switching */
    uint8_t pickleflags;                        /* flags for pickling / unpickling */
} PyStacklessInterpreterState;

#define SLP_INITIAL_TSTATE(tstate) \
    (assert(tstate), \
     assert((tstate)->interp->st.initial_tstate), \
     (tstate)->interp->st.initial_tstate)

/* The complete struct is already initialized with 0.
 * Therefore we only need non-zero initializations.
 */
#define SPL_INTERPRETERSTATE_NEW(interp)       \
    (interp)->st.enable_softswitch = 1;

#define SPL_INTERPRETERSTATE_CLEAR(interp)     \
    (interp)->st.cstack_chain = NULL; /* uncounted ref */  \
    Py_CLEAR((interp)->st.reduce_frame_func);  \
    Py_CLEAR((interp)->st.error_handler);      \
    Py_CLEAR((interp)->st.mem_bomb);           \
    Py_CLEAR((interp)->st.channel_hook);       \
    Py_CLEAR((interp)->st.schedule_hook);      \
    (interp)->st.schedule_fasthook = NULL;     \
    (interp)->st.enable_softswitch = 1;        \
    (interp)->st.pickleflags = 0;

/*
 * Stackless runtime state
 *
 * Initialized by
 * void slp_initialize(struct _stackless_runtime_state * state)
 * in stackless_util.c
 */
struct _stackless_runtime_state {
    /*
     * Note: The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT",
     *       "SHOULD", "SHOULD NOT", "RECOMMENDED",  "MAY", and "OPTIONAL"
     *       in this comment are to be interpreted as described in RFC 2119.
     *
     * 'try_stackless': flag whether the next call into the interpreter should
     *                  try to be stackless
     *
     *   This flags in a way serves as a parameter that we don't have. It can
     *   be accessed as a L-value using the macro '_PyStackless_TRY_STACKLESS'.
     *
     * Possible values of 'try_stackless' / '_PyStackless_TRY_STACKLESS':
     *
     *   0: Stackless calls are not possible, the called function MUST NOT
     *      return Py_UnwindToken.
     *
     *   1: Stackless calls are possible. The called function MUST ensure that
     *      _PyStackless_TRY_STACKLESS is 0 on its return.
     *
     *   other: if the value of _PyStackless_TRY_STACKLESS is the address of the
     *          called function, Stackless calls are possible. The called function
     *
     *          - either MUST NOT modify the value of _PyStackless_TRY_STACKLESS
     *            (case for unmodified C-Python functions).
     *
     *          - or MUST ensure that _PyStackless_TRY_STACKLESS is 0 on its
     *            return (case for stackless aware functions).
     *
     *   (If a stackless call is possible, the called function SHOULD return
     *   Py_UnwindToken and insert an appropriate (C)-frame into the frame chain if
     *   otherwise a recursive call into the Python interpreter would have to be made.
     *   To do so, the called function MAY call a sub-function with
     *   _PyStackless_TRY_STACKLESS set to a non-zero value (see macros STACKLESS_PROMOTE_xxx,
     *   STACKLESS_ASSERT and STACKLESS_VECTORCALL) and return the result of the
     *   sub-function.)
     *
     * The protocol for the caller is:
     *
     *   This flag MAY be only set to 1 if the called thing is stackless aware
     *   (== obeys the stackless-protocol == calls STACKLESS_GETARG() directly
     *   or indirectly). It doesn't matter whether it uses the chance, but it
     *   MUST set _PyStackless_TRY_STACKLESS to zero before returning.
     *
     *   This flag may be set to the address of a directly called C-function.
     *   It is not required, that the called function supports stackless
     *   calls. This variant is used for "vectorcall"-functions (see PEP-590). If
     *   a type supports the vectorcall-protocol, the called C-function not is stored
     *   in a slot of the type object. Instead each instance of the type has its own
     *   function pointer.This makes it impossible to decide if the function to be called
     *   obeys the stackless-protocol and therefore a vectorcall-function MUST NOT
     *   be called with _PyStackless_TRY_STACKLESS set to 1.
     *
     * In theory we could always set _PyStackless_TRY_STACKLESS the address of the
     * called function, but this would not be efficient. A function, that only wraps
     * another stackless-aware function, does not need to use STACKLESS_GETARG(),
     * STACKLESS_PROMOTE_xxx and STACKLESS_ASSERT(). Also some stackless-aware functions
     * are "static inline" and have no address.
     *
     * To prevent leakage of a non zero value of _PyStackless_TRY_STACKLESS to other
     * threads, a thread must reset _PyStackless_TRY_STACKLESS before it drops the GIL.
     * This is done in the C-function drop_gil.
     *
     * As long as the GIL is shared between sub-interpreters and the runtime-state is a
     * global variable, "try_stackless" should be a field in the runtime state.
     * Once the GIL is no longer shared, we should move the flag into the thread state.
     */
    intptr_t try_stackless;

    /* Used to manage free C-stack objects, see stacklesseval.c */
    int cstack_cachecount;
    struct _slp_cstack *cstack_cache[SLP_CSTACK_SLOTS];

    /*
     * Used during a hard switch.
     */
    struct {
        struct _slp_cstack **cstprev;
        struct _slp_cstack *cst;
        struct _slp_tasklet *prev;
    } transfer;
};

#ifdef Py_BUILD_CORE
void slp_initialize(struct _stackless_runtime_state *);
#endif /* #ifdef Py_BUILD_CORE */
