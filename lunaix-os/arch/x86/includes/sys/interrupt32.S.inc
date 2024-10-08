
#define regsize 4

/* stack layout: saved interrupt context */
    .struct 0
idepth:
    .struct idepth + regsize
ieax:
    .struct ieax + regsize
iebx:
    .struct iebx + regsize
iecx:
    .struct iecx + regsize
iedx:
    .struct iedx + regsize
iedi:
    .struct iedi + regsize
iebp:
    .struct iebp + regsize
iesi:
    .struct iesi + regsize
ids:
    .struct ids + regsize
ies:
    .struct ies + regsize
ifs:
    .struct ifs + regsize
igs:
    .struct igs + regsize
iexecp:
iesp:
    .struct iesp + regsize
isave_parent:
    .struct isave_parent + regsize
ivec:
    .struct ivec + regsize
iecode:
    .struct iecode + regsize
ieip:
    .struct ieip + regsize
ics:
    .struct ics + regsize
ieflags:
    .struct ieflags + regsize
iuesp:
    .struct iuesp + regsize
iuss:


/* stack layout: execution (flow-control) state context */
    .struct 0
exsave_prev:
    .struct exsave_prev + regsize
exvec:
    .struct exvec + regsize
execode:
    .struct execode + regsize
exeip:
    .struct exeip + regsize
excs:
    .struct excs + regsize
exeflags:
    .struct exeflags + regsize
exuesp:
    .struct exuesp + regsize
exuss:

/* struct layout: critical section of struct proc_info */
    .struct 0
thread_hstate:
    .struct thread_hstate + regsize
thread_ustack_top:

/* struct layout: proc_sig */
    .struct 0
psig_signum:
    .struct psig_signum + regsize
psig_sigact:
    .struct psig_sigact + regsize
psig_sighand:
    .struct psig_sighand + regsize
psig_saved_hstate: