/* FILE: oxswarn.cc                 -*-Mode: c++-*-
 *
 * Macros and classes for nonfatal message handling.
 *
 */

#include "oxswarn.h"     // Note: Microsoft VC++ 6 blows chunks if
#include "oxsexcept.h"   // oxsexcept.h is included before oxswarn.h
#include "util.h"

/* End includes */

String
Oxs_WarningMessageRevisionInfo::StripRevmarks(const char* revinfo)
{ // Strips CVS tag info from strings
  String work = String(revinfo);
  String::size_type offset = work.find_first_of('$');
  if(offset != String::npos) {
    offset = work.find_first_of(':',offset);
    if(offset != String::npos) {
      offset = work.find_first_not_of(" \t\n",offset+1);
      if(offset != String::npos) {
	String::size_type endset = work.find_last_not_of(" \t\n$");
	if(endset != String::npos) {
	  work.erase(endset+1);
	  work.erase(0,offset-1);
	}
      }
    }
  }
  return work;
}

Oxs_WarningMessageRevisionInfo::Oxs_WarningMessageRevisionInfo
(const char* file_,
 const char* revision_,
 const char* date_,
 const char* revision_author_,
 const char* primary_author_)
{
  file            = String(file_);
  revision        = StripRevmarks(revision_);
  date            = StripRevmarks(date_);
  revision_author = StripRevmarks(revision_author_);
  primary_author  = String(primary_author_);
}

// Static member variables for Oxs_WarningMessage
Oc_Mutex Oxs_WarningMessage::mutex;
int Oxs_WarningMessage::ids_in_use = 0;
std::map<int,int> Oxs_WarningMessage::message_counts;
std::deque<Oxs_WarningMessage::MessageData> Oxs_WarningMessage::message_hold;

int
Oxs_WarningMessage::FormatMessage
(const Oxs_WarningMessageRevisionInfo& revinfo,
 const char* line,
 const char* msg,
 const char* src,
 const char* id,
 String& formatted_message,  // Export
 String& errCode)            // Export
{ // Result code values:
  //  -1 if max_message_count is exceeded
  //   1 if max_message_count is met
  //   0 otherwise
  int result=0;
  {
    Oc_LockGuard lck(mutex);
    // Pull current message count data from message_counts map.
    int current_count=0;
    map<int,int>::iterator id_it = message_counts.find(instance_id);
    if(id_it==message_counts.end()) {
      message_counts[instance_id] = current_count = 1;
    } else {
      current_count = ++(id_it->second);
    }
    if(current_count==max_message_count) {
      result = 1;
    } else if(max_message_count>=0 && current_count>max_message_count) {
      result = -1;
    } 
  }
  if(result<0) return result;

  String fullmsg;
  switch(message_type) {
  case NB_MSG_DEBUG:    fullmsg = String("DEBUG message: "); break;
  case NB_MSG_INFO:     fullmsg = String("INFO message: ");  break;
  case NB_MSG_WARNING:  fullmsg = String("WARNING: ");       break;
  case NB_MSG_ERROR:    fullmsg = String("ERROR: ");         break;
  default:              fullmsg = String("UNKNOWN type: ");  break;
  }
  fullmsg += String(msg)
    + String("\n   in file: ") + revinfo.file
    + String("\n   at line: ") + String(line)
    + String("\n  revision: ") + revinfo.revision
    + String("\n      date: ") + revinfo.date
    + String("\n    author: ") + revinfo.primary_author;

  if(result==1) {
    fullmsg += String("\n*** NOTE: Further messages of this type"
                      " will be discarded. ***");
  }
  formatted_message = fullmsg;

  // Make this an "OOMMF"-style log message
  vector<String> ecvec;
  ecvec.push_back("OOMMF");
  if(src!=NULL) {
    ecvec.push_back(src);
  } else {
    ecvec.push_back("Oxs_WarningMessage");
  }
  if(id!=NULL) {
    ecvec.push_back(id);
  } else {
    char idbuf[512];
    Oc_Snprintf(idbuf,sizeof(idbuf),"Oxs_WarningMessage:%04d",instance_id); 
   ecvec.push_back(idbuf);
  }
  ecvec.push_back(revinfo.file + String(":") + String(line));
  errCode = Nb_MergeList(ecvec);

  return result;
}

int
Oxs_WarningMessage::HoldMessage
(const Oxs_WarningMessageRevisionInfo& revinfo,
 const char* line,
 const char* msg,
 const char* src,
 const char* id)
{ // Can be called from any thread; message is formatted and placed in
  // the message_hold.  When threads are joined the main thread should
  // call TransmitMessageHold to send messages out.
  String formatted_message;
  String errCode;
  int result = FormatMessage(revinfo,line,msg,src,id,
                             formatted_message,errCode);
  if(result<0) return result;
  Oc_LockGuard lck(mutex);
  message_hold.push_back(MessageData(message_type,
                                     formatted_message,errCode));
  return result;
}

int Oxs_WarningMessage::TransmitMessageHold()
{ // Should be called only from main thread.  Return is
  // number of messages transmitted.
  assert(Oxs_IsRootThread());
  int count = 0;
  while(1) {
    MessageData data;
    int isempty=0;
    {
      Oc_LockGuard lck(mutex);
      if(!(isempty=message_hold.empty())) {
        data = message_hold.front();
        message_hold.pop_front();
      }
    }
    if(isempty) break;
    TkMessage(data.msgtype,data.text.c_str(),0,data.errCode.c_str());
    ++count;
  }
  return count;
}

int
Oxs_WarningMessage::Send
(const Oxs_WarningMessageRevisionInfo& revinfo,
 const char* line,
 const char* msg,
 const char* src,
 const char* id)
{
  if(!Oxs_IsRootThread()) {
    // TkMessage call can only be made from the root thread.  If this
    // is not the root thread, then put message into holding area to
    // be called when thread join occurs.
    return HoldMessage(revinfo,line,msg,src,id);
  }
  String formatted_message;
  String errCode;
  int result = FormatMessage(revinfo,line,msg,src,id,
                             formatted_message,errCode);
  if(result<0) return result;
  return TkMessage(message_type,formatted_message.c_str(),0,errCode.c_str());
}

int Oxs_WarningMessage::GetCurrentCount() const
{
  int result=0;
  Oc_LockGuard lck(mutex);
  map<int,int>::const_iterator id_it = message_counts.find(instance_id);
  if(id_it!=message_counts.end()) {
    result = id_it->second;
  }
  return result;
}

void Oxs_WarningMessage::SetCurrentCount(int newcount)
{
  Oc_LockGuard lck(mutex);
  map<int,int>::iterator id_it = message_counts.find(instance_id);
  if(id_it==message_counts.end()) {
    message_counts[instance_id] = newcount;
  } else {
    id_it->second = newcount;
  }
}
