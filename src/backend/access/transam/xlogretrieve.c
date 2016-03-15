#include "postgres.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "miscadmin.h"
#include "postmaster/startup.h"
#include "replication/walsender.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/pmsignal.h"

bool RetrieveXLogFile(char *targetFname, char *path)
{
	char xlogpath[MAXPGPATH];
	char xlogRetrieveCmd[MAXPGPATH];

	/* to be loaded by guc.c */
	const char *retrievepath = XLogRetrievePath;
	int rc;

	if (XLogRetrieveCommand == NULL) 
		return false;

	const char *sp;
	char *dp = xlogRetrieveCmd;
	char *endp = xlogRetrieveCmd + MAXPGPATH - 1;
	*endp = '\0';

	/* We can replace XLOGDIR to some path much safer */
	snprintf(xlogpath, MAXPGPATH, XLOGDIR "/%s", targetFname);

	for(sp = XLogRetrieveCommand; *sp; sp++) {
		if (*sp == '%')
		{
			switch(sp[1]) 
			{
				case 'p': 
					/* %p: relative path of target file */
					sp++;
					StrNCpy(dp, xlogpath, endp - dp);
					make_native_path(dp);
					dp += strlen(dp);
					break;
				case 'f':
					/* %f: filename of desired file */
					sp++;
					StrNCpy(dp, targetFname, endp-dp);
					dp += strlen(dp);
					break;
				case 'a':
					/* %a: archived file directory path */
					sp++;
					StrNCpy(dp, retrievepath, endp-dp);
					make_native_path(dp);
					dp += strlen(dp);
				case '%':
					/* convert %% to a single % */
					sp++;
					if (dp < endp)
						*dp++ = *sp;
					break;
				default:
					/* otherwise treat the % as not special */
					if (dp < endp)
						*dp++ = *sp;
					break;
			}
		}
		else
		{
			if (dp < endp) 
				*dp++ = *sp;
		}
		*dp = '\0';
		ereport(DEBUG3,
				(errmsg_internal("executing retrieve command \"%s\"",
									xlogRetrieveCmd)));
		rc = system(xlogRetrieveCmd);

		if (rc == 0) 
		{
			memcpy(path, xlogpath, MAXPGPATH-1);
			return true;
		}
		return false;
	}


}

void DelRetrievedFile(bool *isRetrieved, char *lastRetrievedFile)
{
	char delCmd[MAXPGPATH]; 
	if (isRetrieved) 
	{
		snprintf(delCmd, MAXPGPATH, "rm -f %s", lastRetrievedFile);	
		system(delCmd);
	}
	isRetrieved = false;
}
