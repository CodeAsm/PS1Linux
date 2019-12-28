/*
 * This file is being included twice - once to build a list of all
 * syscalls and once to build a table of how many arguments each syscall
 * accepts.  Syscalls that receive a pointer to the saved registers are
 * marked as having zero arguments.
 */
SYS(sys_syscall, 0)	/* 0  -  old "setup()" system call*/
SYS(sys_exit, 1)
SYS(sys_fork, 0)
SYS(sys_read, 3)
SYS(sys_write, 3)
SYS(sys_open, 3)			/* 5 */
SYS(sys_close, 3)
SYS(sys_waitpid, 3)
SYS(sys_creat, 2)
SYS(sys_link, 2)
SYS(sys_unlink, 1)		/* 10 */
SYS(sys_execve, 0)
SYS(sys_chdir, 1)
SYS(sys_time, 1)
SYS(sys_mknod, 3)
SYS(sys_chmod, 2)		/* 15 */
SYS(sys_chown16, 3)
SYS(sys_ni_syscall, 0)				/* old break syscall holder */
SYS(sys_stat, 2)
SYS(sys_lseek, 3)
SYS(sys_getpid, 0)		/* 20 */
SYS(sys_mount, 5)
SYS(sys_oldumount, 1)
SYS(sys_setuid16, 1)
SYS(sys_getuid16, 0)
SYS(sys_stime, 1)		/* 25 */
SYS(sys_ptrace, 4)
SYS(sys_alarm, 1)
SYS(sys_fstat, 2)
SYS(sys_pause, 0)
SYS(sys_utime, 2)		/* 30 */
SYS(sys_ni_syscall, 0)				/* old stty syscall holder */
SYS(sys_ni_syscall, 0)				/* old gtty syscall holder */
SYS(sys_access, 2)
SYS(sys_nice, 1)
SYS(sys_ni_syscall, 0)	/* 35 */	/* old ftime syscall holder */
SYS(sys_sync, 0)
SYS(sys_kill, 2)
SYS(sys_rename, 2)
SYS(sys_mkdir, 2)
SYS(sys_rmdir, 1)		/* 40 */
SYS(sys_dup, 1)
SYS(sys_pipe, 0)
SYS(sys_times, 1)
SYS(sys_ni_syscall, 0)				/* old prof syscall holder */
SYS(sys_brk, 1)			/* 45 */
SYS(sys_setgid16, 1)
SYS(sys_getgid16, 0)
SYS(sys_signal, 2)
SYS(sys_geteuid16, 0)
SYS(sys_getegid16, 0)	/* 50 */
SYS(sys_acct, 0)
SYS(sys_umount, 2)					/* recycled never used phys() */
SYS(sys_ni_syscall, 0)				/* old lock syscall holder */
SYS(sys_ioctl, 3)
SYS(sys_fcntl, 3)		/* 55 */
SYS(sys_ni_syscall, 0)				/* old mpx syscall holder */
SYS(sys_setpgid, 2)
SYS(sys_ni_syscall, 0)				/* old ulimit syscall holder */
SYS(sys_ni_syscall, 0)
SYS(sys_umask, 1)		/* 60 */
SYS(sys_chroot, 1)
SYS(sys_ustat, 2)
SYS(sys_dup2, 2)
SYS(sys_getppid, 0)
SYS(sys_getpgrp, 0)		/* 65 */
SYS(sys_setsid, 0)
SYS(sys_sigaction, 3)
SYS(sys_sgetmask, 0)
SYS(sys_ssetmask, 1)
SYS(sys_setreuid16, 2)	/* 70 */
SYS(sys_setregid16, 2)
SYS(sys_sigsuspend, 0)
SYS(sys_sigpending, 1)
SYS(sys_sethostname, 2)
SYS(sys_setrlimit, 2)	/* 75 */
SYS(sys_old_getrlimit, 2)
SYS(sys_getrusage, 2)
SYS(sys_gettimeofday, 2)
SYS(sys_settimeofday, 2)
SYS(sys_getgroups16, 2)	/* 80 */
SYS(sys_setgroups16, 2)
SYS(old_select, 1)
SYS(sys_symlink, 2)
SYS(sys_lstat, 2)
SYS(sys_readlink, 3)		/* 85 */
SYS(sys_uselib, 1)
SYS(sys_swapon, 2)
SYS(sys_reboot, 3)
SYS(old_readdir, 3)
SYS(old_mmap, 6)			/* 90 */
SYS(sys_munmap, 2)
SYS(sys_truncate, 2)
SYS(sys_ftruncate, 2)
SYS(sys_fchmod, 2)
SYS(sys_fchown16, 3)		/* 95 */
SYS(sys_getpriority, 2)
SYS(sys_setpriority, 3)
SYS(sys_ni_syscall, 0)				/* old profil syscall holder */
SYS(sys_statfs, 2)
SYS(sys_fstatfs, 2)		/* 100 */
SYS(sys_ioperm, 3)
SYS(sys_socketcall, 2)
SYS(sys_syslog, 3)
SYS(sys_setitimer, 3)
SYS(sys_getitimer, 2)	/* 105 */
SYS(sys_newstat, 2)
SYS(sys_newlstat, 2)
SYS(sys_newfstat, 2)
SYS(sys_ni_syscall, 0)
SYS(sys_ni_syscall, 0)	/* iopl for i386 */ /* 110 */
SYS(sys_vhangup, 0)
SYS(sys_ni_syscall, 0)	/* obsolete idle() syscall */
SYS(sys_ni_syscall, 0)	/* vm86old for i386 */
SYS(sys_wait4, 4)
SYS(sys_swapoff, 1)		/* 115 */
SYS(sys_sysinfo, 1)
SYS(sys_ipc, 6)
SYS(sys_fsync, 1)
SYS(sys_sigreturn, 0)
SYS(sys_clone, 0)		/* 120 */
SYS(sys_setdomainname, 2)
SYS(sys_newuname, 1)
SYS(sys_cacheflush, 3)	/* modify_ldt for i386 */
SYS(sys_adjtimex, 1)
SYS(sys_mprotect, 3)		/* 125 */
SYS(sys_sigprocmask, 3)
SYS(sys_create_module, 2)
SYS(sys_init_module, 5)
SYS(sys_delete_module, 1)
SYS(sys_get_kernel_syms, 1)	/* 130 */
SYS(sys_quotactl, 0)
SYS(sys_getpgid, 1)
SYS(sys_fchdir, 1)
SYS(sys_bdflush, 2)
SYS(sys_sysfs, 3)		/* 135 */
SYS(sys_personality, 1)
SYS(sys_ni_syscall, 0)	/* for afs_syscall */
SYS(sys_setfsuid16, 1)
SYS(sys_setfsgid16, 1)
SYS(sys_llseek, 5)		/* 140 */
SYS(sys_getdents, 3)
SYS(sys_select, 5)
SYS(sys_flock, 2)
SYS(sys_msync, 3)
SYS(sys_readv, 3)		/* 145 */
SYS(sys_writev, 3)
SYS(sys_getsid, 1)
SYS(sys_fdatasync, 0)
SYS(sys_sysctl, 1)
SYS(sys_mlock, 2)		/* 150 */
SYS(sys_munlock, 2)
SYS(sys_mlockall, 1)
SYS(sys_munlockall, 0)
SYS(sys_sched_setparam,2)
SYS(sys_sched_getparam,2)   /* 155 */
SYS(sys_sched_setscheduler,3)
SYS(sys_sched_getscheduler,1)
SYS(sys_sched_yield,0)
SYS(sys_sched_get_priority_max,1)
SYS(sys_sched_get_priority_min,1)  /* 160 */
SYS(sys_sched_rr_get_interval,2)
SYS(sys_nanosleep,2)
SYS(sys_mremap,4)
SYS(sys_setresuid16, 3)
SYS(sys_getresuid16, 3)	/* 165 */
SYS(sys_ni_syscall, 0)	/* for vm86 */
SYS(sys_query_module, 5)
SYS(sys_poll, 3)
SYS(sys_nfsservctl, 3)
SYS(sys_setresgid16, 3)	/* 170 */
SYS(sys_getresgid16, 3)
SYS(sys_prctl, 5)
SYS(sys_rt_sigreturn, 0)
SYS(sys_rt_sigaction, 4)
SYS(sys_rt_sigprocmask, 4)	/* 175 */
SYS(sys_rt_sigpending, 2)
SYS(sys_rt_sigtimedwait, 4)
SYS(sys_rt_sigqueueinfo, 3)
SYS(sys_rt_sigsuspend, 0)
SYS(sys_pread, 6)		/* 180 */
SYS(sys_pwrite, 6)
SYS(sys_lchown16, 3);
SYS(sys_getcwd, 2)
SYS(sys_capget, 2)
SYS(sys_capset, 2)           /* 185 */
SYS(sys_sigaltstack, 0)
SYS(sys_sendfile, 3)
SYS(sys_ni_syscall, 0)		/* streams1 */
SYS(sys_ni_syscall, 0)		/* streams2 */
SYS(sys_vfork, 1)            /* 190 */
SYS(sys_getrlimit, 2)
SYS(sys_mmap2, 6)
SYS(sys_truncate64, 2)
SYS(sys_ftruncate64, 2)
SYS(sys_stat64, 2)		/* 195 */
SYS(sys_lstat64, 2)
SYS(sys_fstat64, 2)
SYS(sys_chown, 3)
SYS(sys_getuid, 0)
SYS(sys_getgid, 0)		/* 200 */
SYS(sys_geteuid, 0)
SYS(sys_getegid, 0)
SYS(sys_setreuid, 2)
SYS(sys_setregid, 2)
SYS(sys_getgroups, 2)	/* 205 */
SYS(sys_setgroups, 2)
SYS(sys_fchown, 3)
SYS(sys_setresuid, 3)
SYS(sys_getresuid, 3)
SYS(sys_setresgid, 3)	/* 210 */
SYS(sys_getresgid, 3)
SYS(sys_lchown, 3)
SYS(sys_setuid, 1)
SYS(sys_setgid, 1)
SYS(sys_setfsuid, 1)		/* 215 */
SYS(sys_setfsgid, 1)
