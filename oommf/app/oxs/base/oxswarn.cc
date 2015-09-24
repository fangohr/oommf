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


int Oxs_WarningMessage::ids_in_use = 0;
map<int,int> Oxs_WarningMessage::message_counts;

int
Oxs_WarningMessage::Send
(const Oxs_WarningMessageRevisionInfo& revinfo,
 const char* line,
 const char* msg,
 const char* src,
 const char* id)
{
  // Pull current message count data from message_counts map.
  int current_count=0;
  map<int,int>::iterator id_it = message_counts.find(instance_id);
  if(id_it==message_counts.end()) {
    message_counts[instance_id] = current_count = 1;
  } else {
    current_count = ++(id_it->second);
  }
  if(max_message_count>=0 && current_count>max_message_count) return -1;

  String expanded_msg;
  switch(message_type) {
  case NB_MSG_DEBUG:    expanded_msg = String("DEBUG message: "); break;
  case NB_MSG_INFO:     expanded_msg = String("INFO message: ");  break;
  case NB_MSG_WARNING:  expanded_msg = String("WARNING: ");       break;
  case NB_MSG_ERROR:    expanded_msg = String("ERROR: ");         break;
  default:              expanded_msg = String("UNKNOWN type: ");  break;
  }

  expanded_msg += String(msg)
    + String("\n   in file: ") + revinfo.file
    + String("\n   at line: ") + String(line)
    + String("\n  revision: ") + revinfo.revision
    + String("\n      date: ") + revinfo.date
    + String("\n    author: ") + revinfo.primary_author;

#ifndef NDEBUG
  expanded_msg +=
    String("\n last edit: ") + revinfo.revision_author;
  char buf[512];
  Oc_Snprintf(buf,sizeof(buf),"%d",instance_id);
  expanded_msg +=
    String("\n msg    id: ") + String(buf);
  Oc_Snprintf(buf,sizeof(buf),"%d/%d",current_count,max_message_count);
  expanded_msg +=
    String("\n msg count: ") + String(buf);
#endif

  if(current_count==max_message_count) {
    expanded_msg += String("\n*** NOTE: Further messages of this type"
			   " will be discarded. ***");
  }

  // Make this an "OOMMF"-style log message
  const char* errCode_ptr = NULL;
  String errCode;
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
  errCode_ptr = errCode.c_str();

  return TkMessage(message_type,expanded_msg.c_str(),0,errCode_ptr);
}

int Oxs_WarningMessage::GetCurrentCount() const
{
  map<int,int>::const_iterator id_it = message_counts.find(instance_id);
  if(id_it==message_counts.end()) return 0;
  return id_it->second;
}

void Oxs_WarningMessage::SetCurrentCount(int newcount)
{
  map<int,int>::iterator id_it = message_counts.find(instance_id);
  if(id_it==message_counts.end()) {
    message_counts[instance_id] = newcount;
  } else {
    id_it->second = newcount;
  }
}
