/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include "NdbMgmd.hpp"
#include <mgmapi.h>
#include <mgmapi_debug.h>
#include <InputStream.hpp>
#include <signaldata/EventReport.hpp>

/*
  Tests that only need the mgmd(s) started

  Start ndb_mgmd and set NDB_CONNECTSTRING pointing
  to that/those ndb_mgmd(s), then run testMgm
 */


int runTestApiSession(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  Uint64 session_id= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());
  ndb_mgm_connect(h,0,0,0);
  int s= ndb_mgm_get_fd(h);
  session_id= ndb_mgm_get_session_id(h);
  ndbout << "MGM Session id: " << session_id << endl;
  send(s,"get",3,0);
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  struct NdbMgmSession sess;
  int slen= sizeof(struct NdbMgmSession);

  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());
  ndb_mgm_connect(h,0,0,0);

  NdbSleep_SecSleep(1);

  if(ndb_mgm_get_session(h,session_id,&sess,&slen))
  {
    ndbout << "Failed, session still exists" << endl;
    ndb_mgm_disconnect(h);
    ndb_mgm_destroy_handle(&h);
    return NDBT_FAILED;
  }
  else
  {
    ndbout << "SUCCESS: session is gone" << endl;
    ndb_mgm_disconnect(h);
    ndb_mgm_destroy_handle(&h);
    return NDBT_OK;
  }
}

int runTestApiConnectTimeout(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  int result= NDBT_FAILED;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

  ndbout << "TEST connect timeout" << endl;

  ndb_mgm_set_timeout(h, 3000);

  NDB_TICKS  tstart, tend;
  NDB_TICKS secs;

  tstart= NdbTick_CurrentMillisecond();

  ndb_mgm_connect(h,0,0,0);

  tend= NdbTick_CurrentMillisecond();

  secs= tend - tstart;
  ndbout << "Took about: " << secs <<" milliseconds"<<endl;

  if(secs < 4)
    result= NDBT_OK;
  else
    goto done;

  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

  ndbout << "TEST connect timeout" << endl;

  ndb_mgm_destroy_handle(&h);

  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, "1.1.1.1");

  ndbout << "TEST connect timeout (invalid host)" << endl;

  ndb_mgm_set_timeout(h, 3000);

  tstart= NdbTick_CurrentMillisecond();

  ndb_mgm_connect(h,0,0,0);

  tend= NdbTick_CurrentMillisecond();

  secs= tend - tstart;
  ndbout << "Took about: " << secs <<" milliseconds"<<endl;

  if(secs < 4)
    result= NDBT_OK;
  else
    result= NDBT_FAILED;

done:
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return result;
}

int runTestApiTimeoutBasic(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  int result= NDBT_FAILED;
  int cc= 0;
  int mgmd_nodeid= 0;
  ndb_mgm_reply reply;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

  ndbout << "TEST timout check_connection" << endl;
  int errs[] = { 1, 2, 3, -1};

  for(int error_ins_no=0; errs[error_ins_no]!=-1; error_ins_no++)
  {
    int error_ins= errs[error_ins_no];
    ndbout << "trying error " << error_ins << endl;
    ndb_mgm_connect(h,0,0,0);

    if(ndb_mgm_check_connection(h) < 0)
    {
      result= NDBT_FAILED;
      goto done;
    }

    mgmd_nodeid= ndb_mgm_get_mgmd_nodeid(h);
    if(mgmd_nodeid==0)
    {
      ndbout << "Failed to get mgmd node id to insert error" << endl;
      result= NDBT_FAILED;
      goto done;
    }

    reply.return_code= 0;

    if(ndb_mgm_insert_error(h, mgmd_nodeid, error_ins, &reply)< 0)
    {
      ndbout << "failed to insert error " << endl;
      result= NDBT_FAILED;
      goto done;
    }

    ndb_mgm_set_timeout(h,2500);

    cc= ndb_mgm_check_connection(h);
    if(cc < 0)
      result= NDBT_OK;
    else
      result= NDBT_FAILED;

    if(ndb_mgm_is_connected(h))
    {
      ndbout << "FAILED: still connected" << endl;
      result= NDBT_FAILED;
    }
  }

  ndbout << "TEST get_mgmd_nodeid" << endl;
  ndb_mgm_connect(h,0,0,0);

  if(ndb_mgm_insert_error(h, mgmd_nodeid, 0, &reply)< 0)
  {
    ndbout << "failed to remove inserted error " << endl;
    result= NDBT_FAILED;
    goto done;
  }

  cc= ndb_mgm_get_mgmd_nodeid(h);
  ndbout << "got node id: " << cc << endl;
  if(cc==0)
  {
    ndbout << "FAILED: didn't get node id" << endl;
    result= NDBT_FAILED;
  }
  else
    result= NDBT_OK;

  ndbout << "TEST end_session" << endl;
  ndb_mgm_connect(h,0,0,0);

  if(ndb_mgm_insert_error(h, mgmd_nodeid, 4, &reply)< 0)
  {
    ndbout << "FAILED: insert error 1" << endl;
    result= NDBT_FAILED;
    goto done;
  }

  cc= ndb_mgm_end_session(h);
  if(cc==0)
  {
    ndbout << "FAILED: success in calling end_session" << endl;
    result= NDBT_FAILED;
  }
  else if(ndb_mgm_get_latest_error(h)!=ETIMEDOUT)
  {
    ndbout << "FAILED: Incorrect error code (" << ndb_mgm_get_latest_error(h)
           << " != expected " << ETIMEDOUT << ") desc: "
           << ndb_mgm_get_latest_error_desc(h)
           << " line: " << ndb_mgm_get_latest_error_line(h)
           << " msg: " << ndb_mgm_get_latest_error_msg(h)
           << endl;
    result= NDBT_FAILED;
  }
  else
    result= NDBT_OK;

  if(ndb_mgm_is_connected(h))
  {
    ndbout << "FAILED: is still connected after error" << endl;
    result= NDBT_FAILED;
  }
done:
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return result;
}

int runTestApiGetStatusTimeout(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  int result= NDBT_OK;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

  int errs[] = { 0, 5, 6, 7, 8, 9, -1 };

  for(int error_ins_no=0; errs[error_ins_no]!=-1; error_ins_no++)
  {
    int error_ins= errs[error_ins_no];
    ndb_mgm_connect(h,0,0,0);

    if(ndb_mgm_check_connection(h) < 0)
    {
      result= NDBT_FAILED;
      goto done;
    }

    mgmd_nodeid= ndb_mgm_get_mgmd_nodeid(h);
    if(mgmd_nodeid==0)
    {
      ndbout << "Failed to get mgmd node id to insert error" << endl;
      result= NDBT_FAILED;
      goto done;
    }

    ndb_mgm_reply reply;
    reply.return_code= 0;

    if(ndb_mgm_insert_error(h, mgmd_nodeid, error_ins, &reply)< 0)
    {
      ndbout << "failed to insert error " << error_ins << endl;
      result= NDBT_FAILED;
    }

    ndbout << "trying error: " << error_ins << endl;

    ndb_mgm_set_timeout(h,2500);

    struct ndb_mgm_cluster_state *cl= ndb_mgm_get_status(h);

    if(cl!=NULL)
      free(cl);

    /*
     * For whatever strange reason,
     * get_status is okay with not having the last enter there.
     * instead of "fixing" the api, let's have a special case
     * so we don't break any behaviour
     */

    if(error_ins!=0 && error_ins!=9 && cl!=NULL)
    {
      ndbout << "FAILED: got a ndb_mgm_cluster_state back" << endl;
      result= NDBT_FAILED;
    }

    if(error_ins!=0 && error_ins!=9 && ndb_mgm_is_connected(h))
    {
      ndbout << "FAILED: is still connected after error" << endl;
      result= NDBT_FAILED;
    }

    if(error_ins!=0 && error_ins!=9 && ndb_mgm_get_latest_error(h)!=ETIMEDOUT)
    {
      ndbout << "FAILED: Incorrect error code (" << ndb_mgm_get_latest_error(h)
             << " != expected " << ETIMEDOUT << ") desc: "
             << ndb_mgm_get_latest_error_desc(h)
             << " line: " << ndb_mgm_get_latest_error_line(h)
             << " msg: " << ndb_mgm_get_latest_error_msg(h)
             << endl;
      result= NDBT_FAILED;
    }
  }

done:
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return result;
}

int runTestMgmApiGetConfigTimeout(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  int result= NDBT_OK;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

  int errs[] = { 0, 1, 2, 3, -1 };

  for(int error_ins_no=0; errs[error_ins_no]!=-1; error_ins_no++)
  {
    int error_ins= errs[error_ins_no];
    ndb_mgm_connect(h,0,0,0);

    if(ndb_mgm_check_connection(h) < 0)
    {
      result= NDBT_FAILED;
      goto done;
    }

    mgmd_nodeid= ndb_mgm_get_mgmd_nodeid(h);
    if(mgmd_nodeid==0)
    {
      ndbout << "Failed to get mgmd node id to insert error" << endl;
      result= NDBT_FAILED;
      goto done;
    }

    ndb_mgm_reply reply;
    reply.return_code= 0;

    if(ndb_mgm_insert_error(h, mgmd_nodeid, error_ins, &reply)< 0)
    {
      ndbout << "failed to insert error " << error_ins << endl;
      result= NDBT_FAILED;
    }

    ndbout << "trying error: " << error_ins << endl;

    ndb_mgm_set_timeout(h,2500);

    struct ndb_mgm_configuration *c= ndb_mgm_get_configuration(h,0);

    if(c!=NULL)
      free(c);

    if(error_ins!=0 && c!=NULL)
    {
      ndbout << "FAILED: got a ndb_mgm_configuration back" << endl;
      result= NDBT_FAILED;
    }

    if(error_ins!=0 && ndb_mgm_is_connected(h))
    {
      ndbout << "FAILED: is still connected after error" << endl;
      result= NDBT_FAILED;
    }

    if(error_ins!=0 && ndb_mgm_get_latest_error(h)!=ETIMEDOUT)
    {
      ndbout << "FAILED: Incorrect error code (" << ndb_mgm_get_latest_error(h)
             << " != expected " << ETIMEDOUT << ") desc: "
             << ndb_mgm_get_latest_error_desc(h)
             << " line: " << ndb_mgm_get_latest_error_line(h)
             << " msg: " << ndb_mgm_get_latest_error_msg(h)
             << endl;
      result= NDBT_FAILED;
    }
  }

done:
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return result;
}

int runTestMgmApiEventTimeout(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  int result= NDBT_OK;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

  int errs[] = { 10000, 0, -1 };

  for(int error_ins_no=0; errs[error_ins_no]!=-1; error_ins_no++)
  {
    int error_ins= errs[error_ins_no];
    ndb_mgm_connect(h,0,0,0);

    if(ndb_mgm_check_connection(h) < 0)
    {
      result= NDBT_FAILED;
      goto done;
    }

    mgmd_nodeid= ndb_mgm_get_mgmd_nodeid(h);
    if(mgmd_nodeid==0)
    {
      ndbout << "Failed to get mgmd node id to insert error" << endl;
      result= NDBT_FAILED;
      goto done;
    }

    ndb_mgm_reply reply;
    reply.return_code= 0;

    if(ndb_mgm_insert_error(h, mgmd_nodeid, error_ins, &reply)< 0)
    {
      ndbout << "failed to insert error " << error_ins << endl;
      result= NDBT_FAILED;
    }

    ndbout << "trying error: " << error_ins << endl;

    ndb_mgm_set_timeout(h,2500);

    int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP,
                     1, NDB_MGM_EVENT_CATEGORY_STARTUP,
                     0 };

    my_socket my_fd;
#ifdef NDB_WIN
    SOCKET fd= ndb_mgm_listen_event(h, filter);
    my_fd.s= fd;
#else
    int fd= ndb_mgm_listen_event(h, filter);
    my_fd.fd= fd;
#endif

    if(!my_socket_valid(my_fd))
    {
      ndbout << "FAILED: could not listen to event" << endl;
      result= NDBT_FAILED;
    }

    Uint32 theData[25];
    EventReport *fake_event = (EventReport*)theData;
    fake_event->setEventType(NDB_LE_NDBStopForced);
    fake_event->setNodeId(42);
    theData[2]= 0;
    theData[3]= 0;
    theData[4]= 0;
    theData[5]= 0;

    ndb_mgm_report_event(h, theData, 6);

    char *tmp= 0;
    char buf[512];

    SocketInputStream in(my_fd,2000);
    for(int i=0; i<20; i++)
    {
      if((tmp = in.gets(buf, sizeof(buf))))
      {
//        const char ping_token[]="<PING>";
//        if(memcmp(ping_token,tmp,sizeof(ping_token)-1))
          if(tmp && strlen(tmp))
            ndbout << tmp;
      }
      else
      {
        if(in.timedout())
        {
          ndbout << "TIMED OUT READING EVENT at iteration " << i << endl;
          break;
        }
      }
    }

    /*
     * events go through a *DIFFERENT* socket than the NdbMgmHandle
     * so we should still be connected (and be able to check_connection)
     *
     */

    if(ndb_mgm_check_connection(h) && !ndb_mgm_is_connected(h))
    {
      ndbout << "FAILED: is still connected after error" << endl;
      result= NDBT_FAILED;
    }

    ndb_mgm_disconnect(h);
  }

done:
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return result;
}

int runTestMgmApiStructEventTimeout(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  int result= NDBT_OK;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

  int errs[] = { 10000, 0, -1 };

  for(int error_ins_no=0; errs[error_ins_no]!=-1; error_ins_no++)
  {
    int error_ins= errs[error_ins_no];
    ndb_mgm_connect(h,0,0,0);

    if(ndb_mgm_check_connection(h) < 0)
    {
      result= NDBT_FAILED;
      goto done;
    }

    mgmd_nodeid= ndb_mgm_get_mgmd_nodeid(h);
    if(mgmd_nodeid==0)
    {
      ndbout << "Failed to get mgmd node id to insert error" << endl;
      result= NDBT_FAILED;
      goto done;
    }

    ndb_mgm_reply reply;
    reply.return_code= 0;

    if(ndb_mgm_insert_error(h, mgmd_nodeid, error_ins, &reply)< 0)
    {
      ndbout << "failed to insert error " << error_ins << endl;
      result= NDBT_FAILED;
    }

    ndbout << "trying error: " << error_ins << endl;

    ndb_mgm_set_timeout(h,2500);

    int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP,
                     1, NDB_MGM_EVENT_CATEGORY_STARTUP,
                     0 };
    NdbLogEventHandle le_handle= ndb_mgm_create_logevent_handle(h, filter);

    struct ndb_logevent le;
    for(int i=0; i<20; i++)
    {
      if(error_ins==0 || (error_ins!=0 && i<5))
      {
        Uint32 theData[25];
        EventReport *fake_event = (EventReport*)theData;
        fake_event->setEventType(NDB_LE_NDBStopForced);
        fake_event->setNodeId(42);
        theData[2]= 0;
        theData[3]= 0;
        theData[4]= 0;
        theData[5]= 0;

        ndb_mgm_report_event(h, theData, 6);
      }
      int r= ndb_logevent_get_next(le_handle, &le, 2500);
      if(r>0)
      {
        ndbout << "Receieved event" << endl;
      }
      else if(r<0)
      {
        ndbout << "ERROR" << endl;
      }
      else // no event
      {
        ndbout << "TIMED OUT READING EVENT at iteration " << i << endl;
        if(error_ins==0)
          result= NDBT_FAILED;
        else
          result= NDBT_OK;
        break;
      }
    }

    /*
     * events go through a *DIFFERENT* socket than the NdbMgmHandle
     * so we should still be connected (and be able to check_connection)
     *
     */

    if(ndb_mgm_check_connection(h) && !ndb_mgm_is_connected(h))
    {
      ndbout << "FAILED: is still connected after error" << endl;
      result= NDBT_FAILED;
    }

    ndb_mgm_disconnect(h);
  }

done:
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return result;
}

#include <mgmapi_internal.h>

int runSetConfig(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int loops= ctx->getNumLoops();
  for (int l= 0; l < loops; l++){
    g_info << l << ": ";

    struct ndb_mgm_configuration* conf=
      ndb_mgm_get_configuration(mgmd.handle(), 0);
    if (!conf)
      return NDBT_FAILED;

    int r= ndb_mgm_set_configuration(mgmd.handle(), conf);
    free(conf);

    if (r != 0)
      return NDBT_FAILED;
  }
  return NDBT_OK;
}


int runSetConfigUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result= NDBT_OK;
  while(!ctx->isTestStopped() &&
        (result= runSetConfig(ctx, step)) == NDBT_OK)
    ;
  ctx->stopTest();
  return result;
}


int runGetConfig(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int loops= ctx->getNumLoops();
  for (int l= 0; l < loops; l++){
    g_info << l << ": ";
    struct ndb_mgm_configuration* conf=
      ndb_mgm_get_configuration(mgmd.handle(), 0);
    if (!conf)
      return NDBT_FAILED;
    free(conf);
  }
  return NDBT_OK;
}


int runGetConfigUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result= NDBT_OK;
  while(!ctx->isTestStopped() &&
        (result= runGetConfig(ctx, step)) == NDBT_OK)
    ;
  ctx->stopTest();
  return result;
}



int getMgmLogInfo(NdbMgmHandle h, off_t *current_size, off_t *max_size)
{
  int r, ncol;
  char rowbuf[1024];
  char **cols;
  int current_size_colnum= 0;
  int max_size_colnum= 0;

  int rows;
  r= ndb_mgm_ndbinfo(h,"LOGDESTINATION", &rows);

  ncol= ndb_mgm_ndbinfo_colcount(h);

  cols= (char**)malloc(ncol*sizeof(char*));
  for(int i=0;i<ncol;i++)
    cols[i]= (char*) malloc(100*sizeof(char));

  ndb_mgm_ndbinfo_getcolums(h,ncol,100,cols);

  for(int i=0;i<ncol;i++)
  {
    if(strcmp(cols[i],"CURRENT_SIZE")==0)
      current_size_colnum= i;
    if(strcmp(cols[i],"MAX_SIZE")==0)
      max_size_colnum= i;
    free(cols[i]);
  }
  ndbout << endl;

  while(r--)
  {
    ndb_mgm_ndbinfo_getrow(h, rowbuf, sizeof(rowbuf));
    char *col= rowbuf;
    for(int i=0;i<ncol;i++)
    {
      int len;
      col= ndb_mgm_ndbinfo_nextcolumn(col, &len);
      if(!col)
        break;
      if(col[len]=='\'')
        col[len++]='\0';
      col[len]='\0';
      if(i==current_size_colnum)
        *current_size= (off_t)strtoll(col,NULL,10);
      if(i==max_size_colnum)
        *max_size= (off_t)strtoll(col,NULL,10);
      col= &col[len+1];
    }

  }

  ndbout_c("CURRENT SIZE = %lu",*current_size);
  ndbout_c("MAX SIZE = %lu",*max_size);

  free(cols);

  return 0;
}

int runTestMgmLogRotation(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  const char *mgm= mgmd.getConnectString();
  int result= NDBT_FAILED;
  int mgmid= 0;
  off_t current_size= 0, max_size= 0;
  int i,j;

  NdbMgmHandle h= NULL,h1,h2,h3,h4;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgm);

  ndb_mgm_connect(h,0,0,0);

  h1= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h1, mgm);
  ndb_mgm_connect(h1,0,0,0);

  h2= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h2, mgm);
  ndb_mgm_connect(h2,0,0,0);

  h3= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h3, mgm);
  ndb_mgm_connect(h3,0,0,0);

  h4= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h4, mgm);
  ndb_mgm_connect(h4,0,0,0);


  if(ndb_mgm_check_connection(h) < 0)
  {
    result= NDBT_FAILED;
    goto done;
  }

  mgmid= ndb_mgm_get_mgmd_nodeid(h);

  ndbout_c("Connected to MGM server at NodeID: %d",mgmid);

  if(getMgmLogInfo(h, &current_size, &max_size))
  {
    result= NDBT_FAILED;
    goto done;
  }

  for(i=0;i<max_size/4;i++)
  {
        Uint32 theData[25];
        memset(theData,0,sizeof(theData));
        EventReport *fake_event = (EventReport*)theData;

        for(j=0;j<100;j++)
        {
          fake_event->setEventType((Ndb_logevent_type)j);
          fake_event->setNodeId(j+100);
          ndb_mgm_report_event(h, theData, 6);
          fake_event->setNodeId(j+200);
          ndb_mgm_report_event(h1, theData, 6);
          fake_event->setNodeId(j+300);
          ndb_mgm_report_event(h2, theData, 6);
          fake_event->setNodeId(j+400);
          ndb_mgm_report_event(h3, theData, 6);
          fake_event->setNodeId(j+500);
          ndb_mgm_report_event(h4, theData, 6);

        }
        off_t c,m;
        getMgmLogInfo(h, &c, &m);
        if(c < current_size)
        {
          result= NDBT_OK;
          break;
        }
  }

  return result;

done:
  if(h)
    ndb_mgm_destroy_handle(&h);

  return result;
}


int runTestStatus(NDBT_Context* ctx, NDBT_Step* step)
{
  ndb_mgm_node_type types[2] = {
    NDB_MGM_NODE_TYPE_NDB,
    NDB_MGM_NODE_TYPE_UNKNOWN
  };

  NdbMgmd mgmd;
  struct ndb_mgm_cluster_state *state;
  int iterations = ctx->getNumLoops();
  int delay = 2;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int result= NDBT_OK;
  while (iterations-- != 0 && result == NDBT_OK)
  {
    state = ndb_mgm_get_status(mgmd.handle());
    if(state == NULL) {
      ndbout_c("Could not get status!");
      result= NDBT_FAILED;
      continue;
    }
    free(state);

    state = ndb_mgm_get_status2(mgmd.handle(), types);
    if(state == NULL){
      ndbout_c("Could not get status2!");
      result= NDBT_FAILED;
      continue;
    }
    free(state);

    state = ndb_mgm_get_status2(mgmd.handle(), 0);
    if(state == NULL){
      ndbout_c("Could not get status2 second time!");
      result= NDBT_FAILED;
      continue;
    }
    free(state);
  }
  return result;
}


int runTestStatusUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result= NDBT_OK;
  while(!ctx->isTestStopped() &&
        (result= runTestStatus(ctx, step)) == NDBT_OK)
    ;
  ctx->stopTest();
  return result;
}


static bool
get_nodeid(NdbMgmd& mgmd,
           const Properties& args,
           Properties& reply)
{
  // Fill in default values of other args
  Properties call_args(args);
  if (!call_args.contains("version"))
    call_args.put("version", 1);
  if (!call_args.contains("nodetype"))
    call_args.put("nodetype", 1);
  if (!call_args.contains("nodeid"))
    call_args.put("nodeid", 1);
  if (!call_args.contains("user"))
    call_args.put("user", "mysqld");
  if (!call_args.contains("password"))
    call_args.put("password", "mysqld");
  if (!call_args.contains("public key"))
  call_args.put("public key", "a public key");
  if (!call_args.contains("name"))
    call_args.put("name", "testMgm");
  if (!call_args.contains("log_event"))
    call_args.put("log_event", 1);
  if (!call_args.contains("timeout"))
    call_args.put("timeout", 100);

  if (!call_args.contains("endian"))
  {
    union { long l; char c[sizeof(long)]; } endian_check;
    endian_check.l = 1;
    call_args.put("endian", (endian_check.c[sizeof(long)-1])?"big":"little");
  }

  if (!mgmd.call("get nodeid", call_args,
                 "get nodeid reply", reply))
  {
    g_err << "get_nodeid: mgmd.call failed" << endl;
    return false;
  }

  // reply.print();
  return true;
}


static const char*
get_result(const Properties& reply)
{
  const char* result;
  if (!reply.get("result", &result)){
    ndbout_c("result: no 'result' found in reply");
    return NULL;
  }
  return result;
}


static bool result_contains(const Properties& reply,
                            const char* expected_result)
{
  BaseString result(get_result(reply));
  if (strstr(result.c_str(), expected_result) == NULL){
    ndbout_c("result_contains: result string '%s' "
             "didn't contain expected result '%s'",
             result.c_str(), expected_result);
    return false;
  }
  g_info << " result: " << result << endl;
  return true;
}


static bool ok(const Properties& reply)
{
  BaseString result(get_result(reply));
  if (result == "Ok")
    return true;
  return false;
}


static bool get_nodeid_result_contains(NdbMgmd& mgmd,
                                       const Properties& args,
                                       const char* expected_result)
{
  Properties reply;
  if (!get_nodeid(mgmd, args, reply))
    return false;
  return result_contains(reply, expected_result);
}



static bool
check_get_nodeid_invalid_endian1(NdbMgmd& mgmd)
{
  union { long l; char c[sizeof(long)]; } endian_check;
  endian_check.l = 1;
  Properties args;
  /* Set endian to opposite value */
  args.put("endian", (endian_check.c[sizeof(long)-1])?"little":"big");
  return get_nodeid_result_contains(mgmd, args,
                                    "Node does not have the same endian");
}


static bool
check_get_nodeid_invalid_endian2(NdbMgmd& mgmd)
{
  Properties args;
  /* Set endian to weird value */
  args.put("endian", "hepp");
  return get_nodeid_result_contains(mgmd, args,
                                    "Node does not have the same endian");
}


static bool
check_get_nodeid_invalid_nodetype1(NdbMgmd& mgmd)
{
  Properties args;
  args.put("nodetype", 37);
  return get_nodeid_result_contains(mgmd, args,
                                    "unknown nodetype 37");
}


static bool
check_get_nodeid_invalid_nodeid(NdbMgmd& mgmd)
{
  for (int nodeId = MAX_NODES; nodeId < MAX_NODES+2; nodeId++){
    g_info << "Testing invalid node " << nodeId << endl;;

    Properties args;
    args.put("nodeid", nodeId);
    BaseString expected;
    expected.assfmt("No node defined with id=%d", nodeId);
    if (!get_nodeid_result_contains(mgmd, args, expected.c_str()))
      return false;
  }
  return true;
}


static bool
check_get_nodeid_dynamic_nodeid(NdbMgmd& mgmd)
{
  bool result = true;
  Uint32 nodeId= 0; // Get dynamic node id
  for (int nodeType = NDB_MGM_NODE_TYPE_MIN;
       nodeType < NDB_MGM_NODE_TYPE_MAX; nodeType++){
    while(true)
    {
      g_info << "Testing dynamic nodeid " << nodeId
             << ", nodeType: " << nodeType << endl;

      Properties args;
      args.put("nodeid", nodeId);
      args.put("nodetype", nodeType);
      Properties reply;
      if (!get_nodeid(mgmd, args, reply))
        return false;

      /*
        Continue to get dynamic id's until
        an error "there is no more nodeid" occur
      */
      if (!ok(reply)){
        BaseString expected;
        expected.assfmt("No free node id found for %s",
                        NdbMgmd::NodeType(nodeType).c_str());
        if (!result_contains(reply, expected.c_str()))
          result= false; // Got wrong error message
        break;
      }
    }
  }
  return result;
}


static bool
check_get_nodeid_nonode(NdbMgmd& mgmd)
{
  // Find a node that does not exist
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  Uint32 nodeId = 0;
  for(Uint32 i= 1; i < MAX_NODES; i++){
    ConfigIter iter(&conf, CFG_SECTION_NODE);
    if (iter.find(CFG_NODE_ID, i) != 0){
      nodeId = i;
      break;
    }
  }
  if (nodeId == 0)
    return true; // All nodes probably defined

  g_info << "Testing nonexisting node " << nodeId << endl;;

  Properties args;
  args.put("nodeid", nodeId);
  BaseString expected;
  expected.assfmt("No node defined with id=%d", nodeId);
  return get_nodeid_result_contains(mgmd, args, expected.c_str());
}


static bool
check_get_nodeid_nodeid1(NdbMgmd& mgmd)
{

  // Find a node that does exist
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  Uint32 nodeId = 0;
  Uint32 nodeType = NDB_MGM_NODE_TYPE_UNKNOWN;
  for(Uint32 i= 1; i < MAX_NODES; i++){
    ConfigIter iter(&conf, CFG_SECTION_NODE);
    if (iter.find(CFG_NODE_ID, i) == 0){
      nodeId = i;
      iter.get(CFG_TYPE_OF_SECTION, &nodeType);
      break;
    }
  }
  assert(nodeId);
  assert(nodeType != (Uint32)NDB_MGM_NODE_TYPE_UNKNOWN);

  Properties args, reply;
  args.put("nodeid",nodeId);
  args.put("nodetype",nodeType);
  if (!get_nodeid(mgmd, args, reply))
  {
    g_err << "check_get_nodeid_nodeid1: failed for "
          << "nodeid: " << nodeId << ", nodetype: " << nodeType << endl;
    return false;
  }
  reply.print();
  return ok(reply);
}


static bool
check_get_nodeid_wrong_nodetype(NdbMgmd& mgmd)
{
  // Find a node that does exist
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  Uint32 nodeId = 0;
  Uint32 nodeType = NDB_MGM_NODE_TYPE_UNKNOWN;
  for(Uint32 i= 1; i < MAX_NODES; i++){
    ConfigIter iter(&conf, CFG_SECTION_NODE);
    if (iter.find(CFG_NODE_ID, i) == 0){
      nodeId = i;
      iter.get(CFG_TYPE_OF_SECTION, &nodeType);
      break;
    }
  }
  assert(nodeId && nodeType != (Uint32)NDB_MGM_NODE_TYPE_UNKNOWN);

  nodeType = (nodeType + 1) / NDB_MGM_NODE_TYPE_MAX;
  assert(nodeType > NDB_MGM_NODE_TYPE_MIN && nodeType < NDB_MGM_NODE_TYPE_MAX);

  Properties args, reply;
  args.put("nodeid",nodeId);
  args.put("nodeid",nodeType);
  if (!get_nodeid(mgmd, args, reply))
  {
    g_err << "check_get_nodeid_nodeid1: failed for "
          << "nodeid: " << nodeId << ", nodetype: " << nodeType << endl;
    return false;
  }
  BaseString expected;
  expected.assfmt("Id %d configured as", nodeId);
  return result_contains(reply, expected.c_str());
}



int runTestGetNodeId(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int result= NDBT_FAILED;
  if (
      check_get_nodeid_invalid_endian1(mgmd) &&
      check_get_nodeid_invalid_endian2(mgmd) &&
      check_get_nodeid_invalid_nodetype1(mgmd) &&
//      check_get_nodeid_invalid_nodeid(mgmd) &&
      check_get_nodeid_dynamic_nodeid(mgmd) &&
      check_get_nodeid_nonode(mgmd) &&
//      check_get_nodeid_nodeid1(mgmd) &&
      check_get_nodeid_wrong_nodetype(mgmd) &&
      true)
    result= NDBT_OK;

  if (!mgmd.end_session())
    result= NDBT_FAILED;

  return result;
}


int runTestGetNodeIdUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result= NDBT_OK;
  while(!ctx->isTestStopped() &&
        (result= runTestGetNodeId(ctx, step)) == NDBT_OK)
    ;
  ctx->stopTest();
  return result;
}


int runSleepAndStop(NDBT_Context* ctx, NDBT_Step* step)
{
  int counter= 10*ctx->getNumLoops();

  while(!ctx->isTestStopped() && counter--)
    NdbSleep_SecSleep(1);;
  ctx->stopTest();
  return NDBT_OK;
}



NDBT_TESTSUITE(testMgm);
DRIVER(DummyDriver); /* turn off use of NdbApi */
TESTCASE("ApiSessionFailure",
	 "Test failures in MGMAPI session"){
  INITIALIZER(runTestApiSession);

}
TESTCASE("ApiConnectTimeout",
	 "Connect timeout tests for MGMAPI"){
  INITIALIZER(runTestApiConnectTimeout);

}
TESTCASE("ApiTimeoutBasic",
	 "Basic timeout tests for MGMAPI"){
  INITIALIZER(runTestApiTimeoutBasic);

}
TESTCASE("ApiGetStatusTimeout",
	 "Test timeout for MGMAPI getStatus"){
  INITIALIZER(runTestApiGetStatusTimeout);

}
TESTCASE("ApiGetConfigTimeout",
	 "Test timeouts for mgmapi get_configuration"){
  INITIALIZER(runTestMgmApiGetConfigTimeout);

}
TESTCASE("ApiMgmEventTimeout",
	 "Test timeouts for mgmapi get_configuration"){
  INITIALIZER(runTestMgmApiEventTimeout);

}
TESTCASE("ApiMgmStructEventTimeout",
	 "Test timeouts for mgmapi get_configuration"){
  INITIALIZER(runTestMgmApiStructEventTimeout);

}
TESTCASE("SetConfig",
	 "Tests the ndb_mgm_set_configuration function"){
  INITIALIZER(runSetConfig);

}
TESTCASE("GetConfig", "Run ndb_mgm_get_configuration in parallel"){
  STEPS(runGetConfig, 100);
}
#if 0
TESTCASE("MgmLogRotation",
	 "Test log rotation"){
  INITIALIZER(runTestMgmLogRotation);

}
#endif
TESTCASE("TestStatus",
	 "Test status and status2"){
  INITIALIZER(runTestStatus);

}
TESTCASE("TestStatus200",
	 "Test status and status2 with 200 threads"){
  STEPS(runTestStatus, 200);

}
TESTCASE("TestGetNodeId",
	 "Test 'get nodeid'"){
  INITIALIZER(runTestGetNodeId);

}
TESTCASE("Stress",
	 "Run everything while changing config"){
  STEP(runTestGetNodeIdUntilStopped);
  STEP(runSetConfigUntilStopped);
  STEPS(runGetConfigUntilStopped, 10);
  STEPS(runTestStatusUntilStopped, 10);
  STEP(runSleepAndStop);
}
NDBT_TESTSUITE_END(testMgm);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testMgm);
  testMgm.setCreateTable(false);
  testMgm.setRunAllTables(true);
  return testMgm.execute(argc, argv);
}

