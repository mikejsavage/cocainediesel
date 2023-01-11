#include <sys/ptrace.h>
#include <sys/wait.h>

#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

bool Sys_OpenInWebBrowser( const char * url ) {
	int child = fork();
	if( child == -1 )
		return false;

	if( child != 0 )
		return true;

	execlp( "xdg-open", "xdg-open", url, ( char * ) 0 );

	return false;
}

bool Sys_BeingDebugged() {
	pid_t parent_pid = getpid();
	pid_t child_pid = fork();
	if( child_pid == -1 ) {
		err( 1, "fork" );
	}

	if( child_pid == 0 ) {
		// if we can't ptrace the parent then gdb is already there
		if( ptrace( PTRACE_ATTACH, parent_pid, NULL, NULL ) != 0 ) {
			if( errno == EPERM ) {
				printf( "! echo 0 > /proc/sys/kernel/yama/ptrace_scope\n" );
				printf( "! or\n" );
				printf( "! sysctl kernel.yama.ptrace_scope=0\n" );
				_exit( 0 );
			}
			_exit( 1 );
		}

		// ptrace automatically stops the process so wait for SIGSTOP and send PTRACE_CONT
		waitpid( parent_pid, NULL, 0 );
		ptrace( PTRACE_CONT, NULL, NULL );

		// detach
		ptrace( PTRACE_DETACH, parent_pid, NULL, NULL );
		_exit( 0 );
	}

	int status;
	waitpid( child_pid, &status, 0 );
	if( !WIFEXITED( status ) ) {
		_exit( 1 );
	}

	return WEXITSTATUS( status ) == 1;
}

bool IsRenderDocAttached() {
	return dlopen( "librenderdoc.so", RTLD_NOW | RTLD_NOLOAD ) != NULL;
}
