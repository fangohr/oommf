/* FILE: ext.cc                 -*-Mode: c++-*-
 *
 * Base OXS extension class.  This is an abstract class.
 *
 */


#include <limits>
#include <iostream>

#include "oc.h"
#include "nb.h"

#include "director.h"
#include "ext.h"
#include "labelvalue.h"
#include "util.h"
#include "vectorfield.h"

/* End includes */

////////////////////////////////////////////////////////////////////////
// OxsExtUserOutput member functions
void
OxsExtUserOutput::LookupSource
(const char* instance_name)
{ // NB: This function assumes that member variable "director"
  // has been set.
  if(source_field==NULL) {
    // Source field lookup hasn't been performed yet;
    // Do it now, and increment source field cache
    // request.
    if(source_field_name.find(':')==String::npos) {
      // Local reference; change to full name
      source_field_name.insert(0,":");
      source_field_name.insert(0,instance_name);
    }
    Oxs_Output* outcheck =
      director->FindOutputObjectExact(source_field_name.c_str());
    if(outcheck==NULL) {
      String msg = String("Unable to identify source field \"")
        + source_field_name + String("\" for user output \"")
        + name + String("\".");
      throw msg;
    }
    source_field =
      dynamic_cast<Oxs_VectorFieldOutputCache*>(outcheck);
    if(source_field==NULL) {
      String msg= String("Source output \"")
        + source_field_name
        + String("\" requested by user output \"")
        + name
        + String("\" is not a vector field output object.");
      throw msg;
    }

    // Request caching on source field.  This request is
    // decremented when uo is destroyed, provided that
    // source_field is still registered with the director
    // at that time.
    if(source_field->CacheRequestIncrement(1)) {
      source_cacheable = 1;
    } else {
      source_cacheable = 0;
    }

    // If output units were not specified by user, copy value
    // from source.
    if(!units_specified) {
      String source_units = source_field->GetUnits();
      output.ChangeUnits(source_units);
    }

  }
}

OxsExtUserOutput::~OxsExtUserOutput()
{
  if(director!=NULL && source_field!=NULL
     && director->IsRegisteredOutput(source_field)) {
    // source_field pointer is apparently still valid,
    // since Oxs_Output objects are contractually obligated
    // to de-register on destruction.  Therefore, we can
    // decrement the cache request, to pair with the increment
    // that is performed when uo is linked to source_field
    // (cf. Oxs_Ext::Init()).
    if(source_cacheable) {
      source_field->CacheRequestIncrement(-1);
    }
  }
}

////////////////////////////////////////////////////////////////////////
// Oxs_Ext member functions
Oxs_Ext::~Oxs_Ext()
{
#ifdef OC_STL_MAP_BROKEN_EMPTY_DELETE
  // Some STL implementations (MVC++ 6.0 on Windows/Alpha) have a bug
  // that causes a hang if init_strings is destroyed when
  // empty.  To work around this, we just insert a dummy element.
  init_strings[String("foo")] = String("bar");
#endif
}

Oxs_Ext::Oxs_Ext(const char* idname,Oxs_Director* dtr)
  : instanceId(idname), director(dtr) {}

// Helper function for extended constructor (see below)
void Oxs_Ext::AddNewInitStringPair
(const String& key,
 const String& value) 
{ // If key is already in init_strings, throw an exception.
  // Otherwise, add key+value to init_strings.
  map<String,String>::const_iterator it = init_strings.find(key);
  if(it!=init_strings.end()) {
    String msg = String("Duplicate label field \"");
    msg += key;
    msg += String("\" in Specify block.");
    throw Oxs_ExtError(this,msg.c_str());
  }
  init_strings[key] = value;
}

// Extended constructor with built-in argument parsing
Oxs_Ext::Oxs_Ext(const char* idname,Oxs_Director* dtr,
                 const char* argstr)
  : instanceId(idname), director(dtr)
{
  int i;

  // Use Tcl_SplitList to break argstr into components
  Nb_SplitList arglist;
  if(arglist.Split(argstr)!=TCL_OK) {
    // Input string not a valid Tcl list; Throw exception
    String msg = String("Format error in Specify block---"
                        "block is not a valid Tcl list: ");
    msg += String(argstr);
    throw Oxs_ExtError(this,msg.c_str());
  }
  if(arglist.Count()%2 != 0) {
    // Input string is not a list of pairs; Throw exception
    char temp_buf[1024];
    Oc_Snprintf(temp_buf,sizeof(temp_buf),
                "Format error in Specify block---"
                "block should be composed of label+value pairs,"
                " but element count is %d, which is not even: ",
                arglist.Count());
    String msg=String(temp_buf) + String(argstr);
    throw Oxs_ExtError(this,msg.c_str());
  }

  // How many user_outputs are requested?
  OC_INDEX uo_request = 0;
  for(i=0;i<arglist.Count();i+=2) {
    if(strcmp("user_output",arglist[i])==0) ++uo_request;
  }
  user_output.SetSize(uo_request);
  OC_INDEX uo_index = 0;

  // Fill in init_strings key-value array
  for(i=0;i<arglist.Count();i+=2) {
    String key = arglist[i];
    String value = arglist[i+1];

    // Skip "comment" key-value pairs
    if(key.compare("comment")==0) continue;

    // Expand attribute objects
    if(key.compare("attributes")==0) {
      Nb_SplitList split;
      if(split.Split(value.c_str())!=TCL_OK) {
        // Input string not a valid Tcl list; Throw exception
        String msg = String("MIF input parameter value associated"
                            " with label \"");
        msg += key;
        msg += String("\" is not a valid Tcl list: ");
        msg += value;
        throw Oxs_ExtError(this,msg.c_str());
      }
      vector<String> params;
      split.FillParams(params);
      Oxs_OwnedPointer<Oxs_LabelValue> attr;
      OXS_GET_EXT_OBJECT(params,Oxs_LabelValue,attr);

      // "attr" holds label-value pairs, which we now
      // place into init_strings.
      vector<String> labels;
      attr->GetLabels(labels);
      vector<String>::const_iterator it = labels.begin();
      while(it != labels.end()) {
        AddNewInitStringPair(*it,attr->GetValue(*it));
        ++it;
      }
      // "attr" is automatically cleaned up at block exit.
    } else if(key.compare("user_output")==0) {
      // User defined output
      Nb_SplitList pieces;
      if(pieces.Split(value.c_str())!=TCL_OK) {
        // Input string not a valid Tcl list; Throw exception
        String msg = String("MIF input parameter value associated"
                            " with label \"");
        msg += key;
        msg += String("\" is not a valid Tcl list: ");
        msg += value;
        throw Oxs_ExtError(this,msg.c_str());
      }
      if(pieces.Count()%2!=0) {
        char temp_buf[1024];
        Oc_Snprintf(temp_buf,sizeof(temp_buf),
                    "Format error in user_outputs subblock---"
                    "block should be composed of name+value pairs,"
                    " but element count is %d, which is not even.",
                    pieces.Count());
        throw Oxs_ExtError(this,temp_buf);
      }
      if(uo_index>=user_output.GetSize()) {
        throw Oxs_ExtError(this,"Programming error;"
                             " user_output array overflow.");
      }
      OxsExtUserOutput& uo = user_output[uo_index++];
      uo.director = director;
      for(int j=0;j<pieces.Count();j+=2) {
        if(strcmp("comment",pieces[j])==0) continue;
        if(strcmp("name",pieces[j])==0) {
          uo.name = pieces[j+1];
          // Newlines and carriage returns are not allowed in column
          // names.  Replace any such with blanks.
          std::size_t matchindex = 0;
          while((matchindex = uo.name.find_first_of("\n\r",matchindex))
                != std::string::npos) {
            uo.name[matchindex++] = ' ';
          }
        } else if(strcmp("source_field",pieces[j])==0) {
          uo.source_field_name = pieces[j+1];
        } else if(strcmp("select_field",pieces[j])==0) {
          Nb_SplitList tmpparams;
          if(tmpparams.Split(pieces[j+1])!=TCL_OK) {
            char temp_buf[4096];
            strncpy(temp_buf,
                    "Format error in user_output subblock---"
                    "vector field spec for selector is not a proper"
                    " Tcl list: ",
                    sizeof(temp_buf)-1);
            temp_buf[sizeof(temp_buf)-1]='\0'; // Safety
            size_t prefix_size = strlen(temp_buf);
            Oc_EllipsizeMessage(temp_buf+prefix_size,
                                sizeof(temp_buf)-prefix_size,
                                pieces[j+1]);
            throw Oxs_ExtError(this,temp_buf);
          }
          tmpparams.FillParams(uo.selector_init);
        } else if(strcmp("normalize",pieces[j])==0) {
          char* endptr;
          long int result = strtol(pieces[j+1],&endptr,10);
          if(endptr == pieces[j+1] || *endptr!='\0') {
            char item[4000];
            Oc_EllipsizeMessage(item,sizeof(item),pieces[j+1]);
            char temp_buf[5000];
            Oc_Snprintf(temp_buf,sizeof(temp_buf),
                        "Format error in user_output subblock---"
                        "\"normalize\" option value \"%.4000s\""
                        " is not an integer.",
                        item);
            throw Oxs_ExtError(this,temp_buf);
          }
          if(result==0) {
            uo.normalize=0;
          } else {
            uo.normalize=1;
          }
        } else if(strcmp("exclude_0_Ms",pieces[j])==0) {
          char* endptr;
          long int result = strtol(pieces[j+1],&endptr,10);
          if(endptr == pieces[j+1] || *endptr!='\0') {
            char item[4000];
            Oc_EllipsizeMessage(item,sizeof(item),pieces[j+1]);
            char temp_buf[5000];
            Oc_Snprintf(temp_buf,sizeof(temp_buf),
                        "Format error in user_output subblock---"
                        "\"exclude_0_Ms\" option value \"%.4000s\""
                        " is not an integer.",
                        item);
            throw Oxs_ExtError(this,temp_buf);
          }
          if(result==0) {
            uo.exclude_0_Ms=0;
          } else {
            uo.exclude_0_Ms=1;
          }
        } else if(strcmp("user_scaling",pieces[j])==0) {
          OC_BOOL error;
          OC_REAL8m result = Nb_Atof(pieces[j+1],error);
          if(error) {
            char item[4000];
            Oc_EllipsizeMessage(item,sizeof(item),pieces[j+1]);
            char temp_buf[5000];
            Oc_Snprintf(temp_buf,sizeof(temp_buf),
                        "Format error in user_output subblock---"
                        "\"scaling\" option value \"%.4000s\""
                        " is not a floating point value.",
                        item);
            throw Oxs_ExtError(this,temp_buf);
          }
          uo.user_scaling = result;
        } else if(strcmp("units",pieces[j])==0) {
          uo.units = pieces[j+1];
          uo.units_specified = 1;
          // Newlines and carriage returns are not allowed in unit
          // labels.  Replace any such with blanks.
          std::size_t matchindex = 0;
          while((matchindex = uo.units.find_first_of("\n\r",matchindex))
                != std::string::npos) {
            uo.units[matchindex++] = ' ';
          }
        } else {
          char item[4000];
          Oc_EllipsizeMessage(item,sizeof(item),pieces[j]);
          char temp_buf[5000];
          Oc_Snprintf(temp_buf,sizeof(temp_buf),
                      "Format error in user_output subblock---"
                      "option \"%.4000s\" is unknown.",
                      item);
          throw Oxs_ExtError(this,temp_buf);
        }
      }

      // Check that all required options have been specified
      if(uo.name.length()<1) {
        throw Oxs_ExtError(this,"Invalid user_output subblock:"
                             " required field \"name\" is"
                             " missing.");
      }
      if(uo.selector_init.size()<1) {
        throw Oxs_ExtError(this,"Invalid user_output subblock:"
                             " required field \"select_field\" is"
                             " missing.");
      }
      if(uo.source_field_name.length()<1) {
        throw Oxs_ExtError(this,"Invalid user_output subblock:"
                             " required field \"source_field\" is"
                             " missing.");
      }

      // Output object initialization and registration
      uo.output.Setup(this,InstanceName(),uo.name.c_str(),
                      uo.units.c_str(),1,
                      &Oxs_Ext::Fill__user_outputs_init,
                      &Oxs_Ext::Fill__user_outputs,
                      &Oxs_Ext::Fill__user_outputs_fini,
                      &Oxs_Ext::Fill__user_outputs_shares);
      uo.output.Register(director,5);

    } else {
      // Store value against key
      AddNewInitStringPair(key,value);
    }

  }

}


OC_BOOL Oxs_Ext::Init()
{ // NOTE: Any child that overrides the default Oxs_Ext::Init()
  // function should embed a call to Oxs_Ext::Init() for proper
  // initialization of general Oxs_Ext members (in particular,
  // user outputs).

  // Initialize user outputs
  for(OC_INDEX index=0;index<user_output.GetSize();++index) {
    try {
      user_output[index].LookupSource(InstanceName());
    } catch(String& msg) {
      throw Oxs_ExtError(this,msg);
    }
  }

  return 1;
}

void
Oxs_Ext::CheckInitValueParamCount(const String& key,OC_UINT4m count) const
{ // Throws an exception if key is not in init_strings, or if the
  // associated value is not a valid Tcl list, or if the number
  // of parameters in the value is not exactly count.
  map<String,String>::const_iterator it = init_strings.find(key);
  if(it==init_strings.end()) {
    String msg =
      String("MIF input block is missing specification for label \"");
    msg += key;
    msg += String("\"");
    throw Oxs_ExtError(this,msg.c_str());
  }
  const String& value = it->second;
  Nb_SplitList params;
  if(params.Split(value.c_str())!=TCL_OK) {
    // Input string not a valid Tcl list; Throw exception
    String msg = String("MIF input parameter value associated "
                        "with label \"");
    msg += key;
    msg += String("\" is not a valid Tcl list: ");
    msg += value;
    throw Oxs_ExtError(this,msg.c_str());
  }
  if(static_cast<OC_UINT4m>(params.Count())!=count) {
    char item[4000];
    Oc_EllipsizeMessage(item,sizeof(item),key.c_str());
    char buf[5000];
    Oc_Snprintf(buf,sizeof(buf),
                "MIF input block label \"%.4000s\" specification"
                " has %d item(s); it should have exactly %u item(s).",
                item,params.Count(),count);
    throw Oxs_ExtError(this,buf);
  }
}

OC_BOOL Oxs_Ext::HasInitValue(const String& key) const
{
  map<String,String>::const_iterator it = init_strings.find(key);
  if(it==init_strings.end()) return 0;
  return 1;
}



OC_BOOL Oxs_Ext::FindInitValue(const String& key,String& value) const
{
  map<String,String>::const_iterator it = init_strings.find(key);
  if(it==init_strings.end()) return 0;
  value = it->second;
  return 1;
}

OC_BOOL
Oxs_Ext::FindInitValue(const String& key,vector<String>& params) const
{
  String value;
  if(!FindInitValue(key,value)) return 0;
  Nb_SplitList split;
  if(split.Split(value.c_str())!=TCL_OK) {
    // Input string not a valid Tcl list; Throw exception
    String msg = String("MIF input parameter value associated"
                        " with label \"");
    msg += key;
    msg += String("\" is not a valid Tcl list: ");
    msg += value;
    throw Oxs_ExtError(this,msg.c_str());
  }
  split.FillParams(params);
  return 1;
}

void
Oxs_Ext::FindRequiredInitValue(const String& key,String& value) const
{
  if(!FindInitValue(key,value)) {
    String msg = String("Missing required field \"");
    msg += key;
    msg += String("\" in Specify block.");
    throw Oxs_ExtError(this,msg.c_str());
  }
}

void
Oxs_Ext::FindRequiredInitValue
(const String& key,
 vector<String>& params) const
{
  if(!FindInitValue(key,params)) {
    String msg = String("Missing required field \"");
    msg += key;
    msg += String("\" in Specify block.");
    throw Oxs_ExtError(this,msg.c_str());
  }
}

OC_REAL8m Oxs_Ext::GetRealInitValue(const String& key)
{
  String value;
  FindRequiredInitValue(key,value);
  OC_BOOL error;
  OC_REAL8m result = Nb_Atof(value.c_str(),error);
  if(error) {
    String msg = String("Value associated with MIF input label \"");
    msg += key;
    msg += String("\" is not a real number: ");
    msg += value;
    throw Oxs_ExtError(this,msg.c_str());
  }
  DeleteInitValue(key);
  return result;
}

OC_REAL8m Oxs_Ext::GetRealInitValue(const String& key,
                                 OC_REAL8m default_value)
{
  if(!HasInitValue(key)) return default_value;
  return GetRealInitValue(key);
}

ThreeVector Oxs_Ext::GetThreeVectorInitValue(const String& key)
{
  vector<String> value;
  FindRequiredInitValue(key,value);
  if(value.size()!=3) {
    char item[4000];
    Oc_EllipsizeMessage(item,sizeof(item),key.c_str());
    char buf[5000];
    Oc_Snprintf(buf,sizeof(buf),
                "MIF input block label \"%.4000s\" value specification"
                " has %u item(s); it should have exactly 3 item(s).",
                item,(unsigned int)value.size());
    throw Oxs_ExtError(this,buf);
  }
  ThreeVector vec;
  OC_BOOL err1,err2,err3;
  vec.x=Nb_Atof(value[0].c_str(),err1);
  vec.y=Nb_Atof(value[1].c_str(),err2);
  vec.z=Nb_Atof(value[2].c_str(),err3);
  if(err1 || err2 || err3) {
    String msg = String("MIF input block value specification"
                        " associated with label \"");
    msg += key;
    msg += String("\" is not a list of 3 real numbers: ");
    msg += value[0];
    msg += String(" ");
    msg += value[1];
    msg += String(" ");
    msg += value[2];
    throw Oxs_ExtError(this,msg.c_str());
  }
  DeleteInitValue(key);
  return vec;
}

ThreeVector
Oxs_Ext::GetThreeVectorInitValue(const String& key,
                                 const ThreeVector& default_value)
{
  if(!HasInitValue(key)) return default_value;
  return GetThreeVectorInitValue(key);
}

OC_INT4m Oxs_Ext::GetIntInitValue(const String& key)
{
  String value;
  FindRequiredInitValue(key,value);
  char* endptr;
  long int result = strtol(value.c_str(),&endptr,10);
  if(value.empty() || *endptr!='\0') {
    String msg = String("Value associated with MIF input label \"");
    msg += key;
    msg += String("\" is not a integer: ");
    msg += value;
    throw Oxs_ExtError(this,msg.c_str());
  }
  DeleteInitValue(key);
  return static_cast<OC_INT4m>(result);
}

OC_INT4m Oxs_Ext::GetIntInitValue(const String& key,
                               OC_INT4m default_value)
{
  if(!HasInitValue(key)) return default_value;
  return GetIntInitValue(key);
}

OC_UINT4m Oxs_Ext::GetUIntInitValue(const String& key)
{
  String value;
  FindRequiredInitValue(key,value);
  char* endptr;
  unsigned long int result = strtoul(value.c_str(),&endptr,10);
  if(value.empty() || *endptr!='\0') {
    String msg = String("Value associated with MIF input label \"");
    msg += key;
    msg += String("\" is not an unsigned integer: ");
    msg += value;
    throw Oxs_ExtError(this,msg.c_str());
  }
  DeleteInitValue(key);
  return static_cast<OC_UINT4m>(result);
}

OC_UINT4m Oxs_Ext::GetUIntInitValue(const String& key,
                               OC_UINT4m default_value)
{
  if(!HasInitValue(key)) return default_value;
  return GetUIntInitValue(key);
}

String Oxs_Ext::GetStringInitValue(const String& key)
{
  String value;
  FindRequiredInitValue(key,value);
  DeleteInitValue(key);
  return value;
}

String Oxs_Ext::GetStringInitValue(const String& key,
                                   const String& default_value)
{
  if(!HasInitValue(key)) return default_value;
  return GetStringInitValue(key);
}

void
Oxs_Ext::GetRealVectorInitValue
(const String& key,
 OC_UINT4m length,
 vector<OC_REAL8m>& values)
{ // The value associated with key should be a list of "length"
  // real values.  This function converts the value into a vector
  // of OC_REAL8m's, stores the results in export values, and
  // removes the key-value pair from init_strings.  Throws an
  // exception if key in not in init_strings, of if any of the
  // values cannot be completely converted to a OC_REAL8m, or if the
  // parameter list associated with key does not have "length"
  // entries.
  vector<String> strvals;
  FindRequiredInitValue(key,strvals);
  if(strvals.size()!=length) {
    char item[4000];
    Oc_EllipsizeMessage(item,sizeof(item),key.c_str());
    char buf[5000];
    Oc_Snprintf(buf,sizeof(buf),
                "Value for label \"%.4000s\" in Specify block"
                " has wrong number of elements:"
                " %u (should be %u)", item,
                (unsigned int)strvals.size(),(unsigned int)length);
    throw Oxs_ExtError(this,buf);
  }

  values.clear();
  vector<String>::const_iterator it = strvals.begin();
  while(it != strvals.end()) {
    OC_BOOL error;
    values.push_back(Nb_Atof(it->c_str(),error));
    if(error) {
      String msg = String("Entry \"");
      msg += *it;
      msg += String("\" in value list for value \"");
      msg += key;
      msg += String("\" is not a REAL value.");
      throw Oxs_ExtError(this,msg.c_str());
    }
    ++it;
  }

  DeleteInitValue(key);
}

// Group expand/noexpand escape keys
static const char* OXSEXT_EXPAND_KEY   = ":expand:";
static const char* OXSEXT_NOEXPAND_KEY = ":noexpand:";

static void ExpandList(const vector<String>& grouped_list,
                       vector<String>& expanded_list) {
  // The grouped_list import to ExpandList must be a proper
  // grouped list, i.e., it must have at least 2 elements,
  // and the last element must be a positive integer.
  //  ExpandList will examine each element to see if it is
  // also a proper grouped list, and if so make a recursive
  // call to expand out the element.  An element is identified
  // as grouped list if it is a list with at least 2 elements,
  // and the last element is an integer.  For escape purposes,
  // if the last element of a sublist is the string ":noexpand:",
  // then that element is removed and the rest of the sublist
  // is used without expansion.

  if(&grouped_list == &expanded_list) {
    OXS_THROW(Oxs_BadParameter,
              String("Inputs to ExpandList must be "
                     "distinct vector<String>'s"));
  }

  // Check size of group_list
  vector<String>::size_type size = grouped_list.size();
  if(size<2) {
    String msg = String("Invalid grouped list in ExpandList(),"
                        " fewer than 2 elements: ");
    if(size<1) msg += String("{<empty list>}");
    else       msg += grouped_list[0];
    OXS_THROW(Oxs_BadParameter,msg);
  }

  // Extract and check group count
  const char* countstr=grouped_list[size-1].c_str();
  char* endptr;
  long int group_count = strtol(countstr,&endptr,10);
  if(countstr[0]=='\0' || *endptr!='\0' || group_count<1) {
    String msg = String("Invalid group count in ExpandList(): ");
    msg += Nb_MergeList(&grouped_list);
    OXS_THROW(Oxs_BadParameter,msg);
  }

  // Fill dummy_list with one copy of grouped_list
  Nb_SplitList check_list;
  vector<String>::size_type index;
  vector<String> subgroup;
  vector<String> tmplist;
  vector<String> dummy_list;
  for(index=0; index<size-1; index++) {
    if(check_list.Split(grouped_list[index].c_str())!=TCL_OK) {
      String msg = String("Sublist is not a valid Tcl list: ");
      msg += grouped_list[index];
      OXS_THROW(Oxs_BadData,msg);
    }
    int check_list_size = check_list.Count();
    if(check_list_size<2) {
      // Single item
      dummy_list.push_back(grouped_list[index]);
    } else {
      const char* lastitem = check_list[check_list_size-1];
      if(strcmp(lastitem,OXSEXT_NOEXPAND_KEY)==0) {
        // Escape out of recursion.  Throw out trailing
        // OXSEXT_NOEXPAND_KEY element.
        for(int subindex=0;subindex<check_list_size-1;subindex++) {
          dummy_list.push_back(check_list[subindex]);
        }
      } else {
        // See if last item is a positive integer
        char* subendptr;
        long int subgroup_count = strtol(lastitem,&subendptr,10);
        if(lastitem[0]=='\0' || *subendptr!='\0' || subgroup_count<1) {
          // Last item is not a positive integer, so treat
          // whole sublist as a single element.
          dummy_list.push_back(grouped_list[index]);
        } else {
          // Sub-group. Make recursive call
          check_list.FillParams(subgroup);
          ExpandList(subgroup,tmplist); // <<< RECURSIVE CALL >>>
          dummy_list.insert(dummy_list.end(),
                            tmplist.begin(),tmplist.end());
        }
      }
    }
  }

  // Make requested number (group_count) of copies
  expanded_list.clear();
  expanded_list.reserve(group_count*dummy_list.size());
  for(long int copy_number=0; copy_number<group_count; copy_number++) {
    expanded_list.insert(expanded_list.end(),
                         dummy_list.begin(),
                         dummy_list.end());
  }

}

void
Oxs_Ext::GetGroupedStringListInitValue
(const String& key,
 vector<String>& expanded_list)
{
  // The value associated with key should be a grouped list of strings.
  // This function loads the strings into the export vector<string>
  // expanded_list, and removes the key-value pair from init_strings.
  // An exception is thrown if key in not in init_strings, if the
  // associated value does not contain at least one element, of if there
  // is a structural problem with the grouped list.  If the associated
  // value is a list of at least 2 elements, and if the last element is
  // ":expand:", then that last element is pitched and the remainder is
  // expanded as a grouped sublist.  Grouped sublists have the form
  // {x1 x2 ... xn R} where x1 through xn are strings, and R is an
  // integer repeat count.  The sublists are expanded recursively, so,
  // i.e., xi can also be either an individual value or a grouped
  // sublist of the same form.  As an example, the value "fred {barney
  // {wilma betty 3} 2} :expand:" would be expanded into "fred barney
  // wilma betty wilma betty wilma betty barney wilma betty wilma betty
  // wilma betty".  Note that there is no group count expected or
  // allowed at the top level, i.e., to get the expanded list "fred
  // fred fred" an extra level of grouping is necessary, i.e.,
  // "{fred 3} :expand:" is correct, not "fred 3 :expand:".
  //   If a sublist element has embedded spaces, and the last bit can be
  // interpreted as an integer, then the algorithm outlined above will
  // cause the sublist to be improperly expanded.  For example, consider
  // the flat list "{fred 3} {fred 3}".  This is *not* equivalent to the
  // grouped list "{{fred 3} 2} :expand:", because as a result of
  // recursive expansion, the latter expands into "fred fred fred fred
  // fred fred".  To disable recursive expansion use the ":noexpand:"
  // keyword in the repeat count location.  The proper grouped list
  // representation of "{fred 3} {fred 3}" is
  //             "{{{fred 3} :noexpand:} 2} :expand:"
  // The ":noexpand:" keyword may also be used at the top-level to force
  // a flat, i.e., unexpanded list.  As explained above, no-expansion is
  // the default, but ":noexpand:" can be used to handle the situation
  // where a flat list by chance has ":expand:" as the last element.  The
  // ":noexpand:" keyword may be safely added to any flat list, which also
  // protects against accidental eating of a chance ":noexpand:" last
  // element in a flat list.  This is mainly useful in lists generated
  // by code from unknown input.

  vector<String> strvals;
  FindRequiredInitValue(key,strvals);
  if(strvals.empty()) {
    String msg = String("Value for label \"");
    msg += key;
    msg += String("\" in Specify block is an empty list.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  // Is this a grouped list?
  if(strvals.size()<2 || strvals.back().compare(OXSEXT_EXPAND_KEY)!=0) {
    // Flat list.  If last element is OXSEXT_NOEXPAND_KEY, remove it.
    if(strvals.back().compare(OXSEXT_NOEXPAND_KEY)==0) {
      strvals.pop_back();
    }
    swap(expanded_list,strvals);
  } else {
    // Grouped list.  Replace OXSEXT_EXPAND_KEY keyword with the
    // implicit full string group count of 1.
    strvals.back() = String("1");
    
    // Expand
    try {
      ExpandList(strvals,expanded_list);
    } catch (Oxs_Exception& errmsg) {
      String msg = String("Error expanding group structure for label \"");
      msg += key;
      msg += String("\": ");
      msg += String(errmsg.MessageType());
      msg += String(":\n");
      msg += errmsg.MessageText();
      throw Oxs_ExtError(this,msg.c_str());
    }
  }

  DeleteInitValue(key);
}

void
Oxs_Ext::GetGroupedRealListInitValue
(const String& key,
 vector<OC_REAL8m>& export_list)
{ // Analogous to GetGroupedStringListInitValue(), but the final
  // expanded list should consist entirely of OC_REAL8m values.  An
  // exception is thrown if key in not in init_strings, if the
  // associated value does not contain at least one element, if there is
  // a structural problem with the grouped list, of if any of the values
  // in the expanded_list cannot be completely converted to a OC_REAL8m
  // value. See notes in the GetGroupedStringListInitValue() function
  // for additional details.
  vector<String> strvals;
  FindRequiredInitValue(key,strvals);
  if(strvals.empty()) {
    String msg = String("Value for label \"");
    msg += key;
    msg += String("\" in Specify block is an empty list.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  vector<String> expanded_list;

  // Is this a grouped list?
  if(strvals.size()<2 || strvals.back().compare(OXSEXT_EXPAND_KEY)!=0) {
    // Flat list.  If last element is OXSEXT_NOEXPAND_KEY, remove it.
    if(strvals.back().compare(OXSEXT_NOEXPAND_KEY)==0) {
      strvals.pop_back();
    }
    swap(expanded_list,strvals);
  } else {
    // Grouped list.  Replace OXSEXT_EXPAND_KEY keyword with the
    // implicit full string group count of 1.
    strvals.back() = String("1");
    try {
      ExpandList(strvals,expanded_list);
    } catch (Oxs_Exception& errmsg) {
      String msg = String("Error expanding group structure for label \"");
      msg += key;
      msg += String("\": ");
      msg += String(errmsg.MessageType());
      msg += String(":\n");
      msg += errmsg.MessageText();
      throw Oxs_ExtError(this,msg.c_str());
    }
  }

  export_list.clear();
  vector<String>::const_iterator it = expanded_list.begin();
  while(it != expanded_list.end()) {
    OC_BOOL error;
    export_list.push_back(Nb_Atof(it->c_str(),error));
    if(error) {
      String msg = String("Entry \"");
      msg += *it;
      msg += String("\" in value list for label \"");
      msg += key;
      msg += String("\" is not a REAL value.");
      throw Oxs_ExtError(this,msg.c_str());
    }
    ++it;
  }
#ifdef OC_OPEN_WATCOM_BROKEN_VECTOR_RETURNS
  export_list.size();  // Bug hack
#endif

  DeleteInitValue(key);
}

void
Oxs_Ext::GetGroupedIntListInitValue
(const String& key,
 vector<OC_INT4m>& export_list)
{ // Analogous to GetGroupedStringListInitValue(), but the final
  // expanded list should consist entirely of OC_INT4m values.  An
  // exception is thrown if key in not in init_strings, if the
  // associated value does not contain at least one element, if there is
  // a structural problem with the grouped list, of if any of the values
  // in the expanded_list cannot be completely converted to an OC_INT4m
  // value. See notes in the GetGroupedStringListInitValue() function
  // for additional details.
  vector<String> strvals;
  FindRequiredInitValue(key,strvals);
  if(strvals.empty()) {
    String msg = String("Value for label \"");
    msg += key;
    msg += String("\" in Specify block is an empty list.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  vector<String> expanded_list;

  // Is this a grouped list?
  if(strvals.size()<2 || strvals.back().compare(OXSEXT_EXPAND_KEY)!=0) {
    // Flat list.  If last element is OXSEXT_NOEXPAND_KEY, remove it.
    if(strvals.back().compare(OXSEXT_NOEXPAND_KEY)==0) {
      strvals.pop_back();
    }
    swap(expanded_list,strvals);
  } else {
    // Grouped list.  Replace OXSEXT_EXPAND_KEY keyword with the
    // implicit full string group count of 1.
    strvals.back() = String("1");
    try {
      ExpandList(strvals,expanded_list);
    } catch (Oxs_Exception& errmsg) {
      String msg = String("Error expanding group structure for label \"");
      msg += key;
      msg += String("\": ");
      msg += String(errmsg.MessageType());
      msg += String(":\n");
      msg += errmsg.MessageText();
      throw Oxs_ExtError(this,msg.c_str());
    }
  }

  export_list.clear();
  vector<String>::const_iterator it = expanded_list.begin();
  while(it != expanded_list.end()) {
    char* endptr;
    const char* cptr=it->c_str();
    long int iresult = strtol(cptr,&endptr,10);
    if(cptr[0]=='\0' || *endptr!='\0') {
      // Error!
      String msg = String("Entry \"");
      msg += *it;
      msg += String("\" in value list for label \"");
      msg += key;
      msg += String("\" is not an INT value.");
      throw Oxs_ExtError(this,msg.c_str());
    } else {
      export_list.push_back(static_cast<OC_UINT4m>(iresult));
    }
    ++it;
  }
#ifdef OC_OPEN_WATCOM_BROKEN_VECTOR_RETURNS
  export_list.size();  // Bug hack
#endif

  DeleteInitValue(key);
}

void
Oxs_Ext::GetGroupedUIntListInitValue
(const String& key,
 vector<OC_UINT4m>& export_list)
{ // Analogous to GetGroupedStringListInitValue(), but the final
  // expanded list should consist entirely of OC_UINT4m values.  An
  // exception is thrown if key in not in init_strings, if the
  // associated value does not contain at least one element, if there is
  // a structural problem with the grouped list, of if any of the values
  // in the expanded_list cannot be completely converted to an OC_UINT4m
  // value. See notes in the GetGroupedStringListInitValue() function
  // for additional details.
  vector<String> strvals;
  FindRequiredInitValue(key,strvals);
  if(strvals.empty()) {
    String msg = String("Value for label \"");
    msg += key;
    msg += String("\" in Specify block is an empty list.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  vector<String> expanded_list;

  // Is this a grouped list?
  if(strvals.size()<2 || strvals.back().compare(OXSEXT_EXPAND_KEY)!=0) {
    // Flat list.  If last element is OXSEXT_NOEXPAND_KEY, remove it.
    if(strvals.back().compare(OXSEXT_NOEXPAND_KEY)==0) {
      strvals.pop_back();
    }
    swap(expanded_list,strvals);
  } else {
    // Grouped list.  Replace OXSEXT_EXPAND_KEY keyword with the
    // implicit full string group count of 1.
    strvals.back() = String("1");
    try {
      ExpandList(strvals,expanded_list);
    } catch (Oxs_Exception& errmsg) {
      String msg = String("Error expanding group structure for label \"");
      msg += key;
      msg += String("\": ");
      msg += String(errmsg.MessageType());
      msg += String(":\n");
      msg += errmsg.MessageText();
      throw Oxs_ExtError(this,msg.c_str());
    }
  }

  export_list.clear();
  vector<String>::const_iterator it = expanded_list.begin();
  while(it != expanded_list.end()) {
    char* endptr;
    const char* cptr=it->c_str();
    unsigned long int uiresult = strtoul(cptr,&endptr,10);
    if(cptr[0]=='\0' || *endptr!='\0') {
      // Error!
      String msg = String("Entry \"");
      msg += *it;
      msg += String("\" in value list for label \"");
      msg += key;
      msg += String("\" is not an UINT value.");
      throw Oxs_ExtError(this,msg.c_str());
    } else {
      export_list.push_back(static_cast<OC_UINT4m>(uiresult));
    }
    ++it;
  }
#ifdef OC_OPEN_WATCOM_BROKEN_VECTOR_RETURNS
  export_list.size();  // Bug hack
#endif

  DeleteInitValue(key);
}

OC_BOOL
Oxs_Ext::DeleteInitValue(const String& key)
{ // Removes the specified key-value pair from init_strings.
  //  Returns 1 on success, 0 if key was not found in init_strings.
  // It seems that 'init_strings.erase(key)' should work here, but
  //  this produces a NULL-pointer de-reference under MVC++ 6.0 on
  //  Windows/Alpha.  So we just use find() and erase(it) instead.
  map<String,String>::iterator it = init_strings.find(key);
  if(it==init_strings.end()) return 0;
  init_strings.erase(it);
  return 1;
}

// The following static function is wrapped by member function
// Oxs_Ext::FindOrCreateObject.  The basic functionality
// is pulled into this separate routine to make it available
// outside of Oxs_Ext objects.
//   On success, fills import obj.  On error, throws a String
// containing an error message.
void Oxs_Ext::FindOrCreateObjectHelper
(Oxs_Director* dtr,
 const vector<String>& params,
 Oxs_OwnedPointer<Oxs_Ext>& obj,
 const char* subtype)
{
  Oxs_Ext* objptr=NULL;
  // Check for explicit reference to existing object
  OC_UINT4m match_count=0;
  if(params.size()==1 &&
     (objptr = dtr->FindExtObject(params[0],match_count))!=NULL) {
    obj.SetAsNonOwner(objptr);
    return;
  }
  if(match_count>1) {
      String msg
        = "Multiple matches found for Oxs_Ext object identifier \"";
      msg += params[0];
      msg += "\"";
      throw msg;
  }

  // Otherwise, try to create a new Oxs_Ext object
  String object_type;
  String object_initstring;
  if(params.size()==1) {
    OC_BOOL err;
    Nb_Atof(params[0].c_str(),err);
    if(subtype==NULL || strcmp(subtype,"Oxs_ScalarField")!=0 || err) {
      // See note below about Intel C++ 5 compiler
      String msg = "Unable to find pre-existing Oxs_Ext object ";
      msg += params[0];
      throw msg;
    }
    // Else, implicit Oxs_UniformScalarField object creation request
    object_type = String("Oxs_UniformScalarField");
    object_initstring = String("value ");
    object_initstring += params[0];
  } else if(params.size()==2) {
    // Explicit Oxs_Ext object creation request
    object_type = params[0];
    object_initstring = params[1];
  } else if(params.size()==3 && subtype!=NULL &&
            strcmp(subtype,"Oxs_VectorField")==0) {
    // Implicit Oxs_UniformVectorField object creation request
    object_type = String("Oxs_UniformVectorField");
    vector<String> tmp_sublist;
    tmp_sublist.push_back("vector");
    tmp_sublist.push_back(Nb_MergeList(params));
    object_initstring = Nb_MergeList(tmp_sublist);
  } else {
    // Note: The following construction of msg is broken up
    // into 3 separate statements, because when merged together
    // as a single statement with multiple +'s the Intel C++ 5
    // compiler deletes the string temporary associated with
    // InstanceName() too soon.
    String msg = String("Invalid parameter string detected in "
                        "Oxs_Ext reference-or-create sub-block; ");
    if(subtype!=NULL && strcmp(subtype,"Oxs_VectorField")==0) {
      msg += String("Wrong number of parameters (should be 1-3)");
    } else {
      msg += String("Wrong number of parameters (should be 1 or 2)");
    }
    throw msg;
  }

  // If this point is reached, then a new object is requested.
  // The type is held in object_type, and the initialization
  // string is in object_initstring.
  try {
    objptr = MakeNew(object_type.c_str(),dtr,
                     object_initstring.c_str());
  } catch (Oxs_ExtError& err) {
    // See note above about Intel C++ 5 compiler
    String msg = "Unable to create inline Oxs_Ext object ";
    msg += params[0];
    msg += String(" with args ");
    msg += params[1];
    msg += String(" --- \n");
    msg += String(err);
    throw msg;
  }
  obj.SetAsOwner(objptr);
}

void
Oxs_Ext::FindOrCreateObject
(const vector<String>& params,
 Oxs_OwnedPointer<Oxs_Ext>& obj,
 const char* subtype) const
{
  try {
    FindOrCreateObjectHelper(director,params,obj,subtype);
  } catch(String errmsg) {
      String msg = "In Specify block for ";
      msg += String(InstanceName());
      msg += String(": ");
      msg += errmsg;
      throw Oxs_ExtError(this,msg.c_str());
  }
}

void
Oxs_Ext::FindOrCreateInitObject
(const String& label,
 const char* subtype,
 Oxs_OwnedPointer<Oxs_Ext>& obj) const
{ // This routine is intended to be used in conjuction with the
  // OXS_GET_INIT_EXT_OBJECT macro, which performs the final
  // downcast.  Error handling is taken care of here, as opposed
  // to inside the macro, because some versions of gcc (at least
  // egcs-2.91.66 on Linux/AXP) use up vast amounts of memory
  // processing the try/catch blocks.  Actually, I don't know if
  // the problem is the try/catch blocks directly, or the string
  // template shenanigans.
  try {
    vector<String> params;
    FindRequiredInitValue(label,params);
    FindOrCreateObject(params,obj,subtype);
  } catch (Oxs_ExtError& err) {
    err.Prepend(String("Error processing label \"")
                + label + String("\" --- "));
    throw;
  } catch (Oxs_Exception& err) {
    err.Prepend(String("Error processing label \"")
                + label + String("\" --- "));
    throw;
  } catch (Oc_Exception& err) {
    String foomsg = String("Error processing label \"")
                  + label + String("\" --- ");
    err.PrependMessage(foomsg.c_str());
    throw;
  } catch (EXCEPTION& err) {
    String msg
      = String("System library error processing label \"")
      + label + String("\" --- ") + String(err.what());
    throw Oxs_ExtError(this,msg);
  } catch (...) {
    String msg = String("Unrecognized error processing label \"")
      + label + String("\".");
    throw Oxs_ExtError(this,msg);
  }
}

void Oxs_Ext::VerifyAllInitArgsUsed()
{ // Throws an exception if GetInitValueCount() != 0.  Use this in
  // combination with DeleteInitValue() to check MIF input block for
  // superfluous parameters.  This should be called in only the actual
  // instantiating class (not a parenting class).
  if(GetInitValueCount()!=0) {
    String msg =  String("Specify block includes the following"
                         " unrecognized or unused parameters:\n");
    map<String,String>::const_iterator it = init_strings.begin();
    while(it!=init_strings.end()) {
      vector<String> paramline;
      paramline.push_back(it->first);
      paramline.push_back(it->second);
      msg += "   " + Nb_MergeList(&paramline) + "\n";
      ++it;
    }
    throw Oxs_ExtError(this,msg.c_str());
  }
}

String Oxs_Ext::ExtractClassName(const String& name)
{
  String classname=name;
  String::size_type endpt=classname.find(':');
  if(endpt!=String::npos) {
    classname.resize(endpt); // colon found; truncate
  }
  return classname;
}

String Oxs_Ext::ExtractInstanceTail(const String& name)
{
  String::size_type index = name.find(':');
  String tail; // Default instance tail is empty string
  if(index!=String::npos) {
    tail = name.c_str()+index+1; // Copy tail
  }
  return tail;
}

OC_BOOL Oxs_Ext::ValidateIdStrings() const
{
  String classrequest=ExtractClassName(instanceId);
  if(classrequest.compare(ClassName())!=0) {
    // Error, wrong type!
    String msg = String("Incorrect object type request (");
    msg += classrequest;
    msg += String(") in ");
    msg += String(ClassName());
    msg += String(" constructor.");
    throw Oxs_ExtError(this,msg.c_str());
  }
  return 1;
}


// Oxs_Ext Registration code /////////////////////////////////////

Oxs_ExtRegistrationBlock::Oxs_ExtRegistrationBlock
(const char* name,Oxs_ExtChildConstructFunc* ctr)
  : classid(name), construct(ctr)
{
  Oxs_Ext::Register(*this);
}

// Function that returns a reference to an STL "map" of all registered
// classes.  The map is a static variable of RegClasses(). This insures
// the map is initialized before it is used, which would otherwise be
// difficult, because Oxs_Ext child classes call Oxs_Ext::Register
// during static initialization.  (See Section 9.4.1 of "The C++
// Programming Lanuage," Bjarne Stroustrup, 3rd edition for a discussion
// of nonlocal variable initialization order.)
map<String,Oxs_ExtRegistrationBlock>& Oxs_Ext::RegClasses()
{
  static map<String,Oxs_ExtRegistrationBlock> regclasses;
  return regclasses;
}

void Oxs_Ext::Register(Oxs_ExtRegistrationBlock const& regblk)
{
  map<String,Oxs_ExtRegistrationBlock>& regclasses=RegClasses();
  regclasses[regblk.classid]=regblk;
}

Oxs_Ext* Oxs_Ext::MakeNew
(const char* idstring, // Child object name
 Oxs_Director* dtr,    // App director
 const char* argstr)   // MIF input block params
{
  map<String,Oxs_ExtRegistrationBlock>& regclasses=RegClasses();
  String classname(ExtractClassName(idstring));
  map<String,Oxs_ExtRegistrationBlock>::iterator it =
    regclasses.find(classname);
  if(it==regclasses.end()) {
    String msg = String("In Oxs_Ext::MakeNew();"
                        " unrecognized object class: ");
    msg += classname;
    throw Oxs_ExtErrorUnregisteredType(msg.c_str());
  }
  Oxs_Ext* obj=NULL;
  try {
    obj=it->second.construct(idstring,dtr,argstr);
    obj->ValidateIdStrings();  // Verify we got the right one! (Safety)
  }
  catch (Oxs_ExtError& err) {
    throw Oxs_ExtErrorConstructFailure(err.c_str());
  }
  return obj;
}

String Oxs_Ext::ListRegisteredChildClasses()
{
  map<String,Oxs_ExtRegistrationBlock>& regclasses=RegClasses();
  String result("Oxs Registration List ---");
  map<String,Oxs_ExtRegistrationBlock>::iterator it;
  for(it=regclasses.begin();it!=regclasses.end();++it) {
    result.append("\n ");
    result.append(it->first);
  }
  return result;
}


void
Oxs_Ext::StageRequestCount(unsigned int& min,unsigned int& max) const
{
  min=0;  max=UINT_MAX; // No restriction.
}

void
Oxs_Ext::Fill__user_outputs_init
(const Oxs_SimState& state,int number_of_threads)
{ // NOTE 1: This code assumes that the cells in the mesh
  //         are uniformly sized.
  // NOTE 2: This code assumes that Ms is fixed.
#ifndef NDEBUG
  if(!state.mesh->HasUniformCellVolumes()) {
    throw Oxs_ExtError(this,"NEW CODE REQUIRED: Current"
                         " user outputs require meshes "
                         " with uniform cell sizes, such as "
                         "Oxs_RectangularMesh.");
  }
#endif

  const OC_INDEX size = state.mesh->Size();
  for(OC_INDEX index=0;index<user_output.GetSize();++index) {
    OxsExtUserOutput& uo = user_output[index];
    Oxs_ChunkScalarOutput<Oxs_Ext>& output = uo.output;
    if(output.cache.state_id == state.Id()) {
      continue;  // Already done
    }

    output.cache.state_id = 0;

    if(uo.source_field==NULL) {
      // Source field lookup hasn't been performed yet;
      // Do it now.
      uo.LookupSource(InstanceName());
    }

    // Initialize source field.  When chunk vector fields
    // are coded, call their initializer instead.
    if(uo.source_field->cache.state_id != state.Id()) {
      if(uo.source_cacheable) {
        if(!uo.source_field->UpdateCache(&state)) {
          char item1[4000];
          Oc_EllipsizeMessage(item1,sizeof(item1),
                              uo.source_field_name.c_str());
          char item2[4000];
          Oc_EllipsizeMessage(item2,sizeof(item2),
                              uo.name.c_str());
          char buf[9000];
          Oc_Snprintf(buf,sizeof(buf),
                      "Source output \"%.4000s\" for user output"
                      " \"%.4000s\" has not been and is unable to"
                      " be updated for state %d (source field"
                      " state id = %d)",
                      item1,item2,
                      state.Id(),uo.source_field->cache.state_id);
          throw Oxs_ExtError(this,buf);
        }
      } else {
        uo.source_field->FillBuffer(&state,uo.source_buffer);
      }
    }

    // Initialize selector and selector scaling
    if(!uo.selector.CheckMesh(state.mesh)) {
      Oxs_OwnedPointer<Oxs_VectorField> tmpinit; // Initializer
      OXS_GET_EXT_OBJECT(uo.selector_init,
                         Oxs_VectorField,tmpinit);
      tmpinit->FillMeshValue(state.mesh,uo.selector);

      OC_INDEX cell_count = size;
      if(uo.exclude_0_Ms) {
        const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
        cell_count = 0;
        for(OC_INDEX j=0;j<size;++j) {
          if(0.0==Ms[j]) {
            uo.selector[j].Set(0,0,0);
          } else {
            ++cell_count;
          }
        }
      }

      uo.scaling = 1.0; // safety
      if(uo.normalize) {
        OC_REAL8m sum=0.0;
        for(OC_INDEX j=0;j<size;++j) {
          sum += sqrt(uo.selector[j].MagSq());
        }
        if(sum>0.0) uo.scaling = 1.0/sum;
      } else {
        if(size>0.0) uo.scaling = 1.0/static_cast<OC_REAL8m>(cell_count);
      }
      uo.scaling *= uo.user_scaling;

    }

    // Set up thread results vector for each output.
    std::vector<OC_REAL8m>& chunk = uo.chunk_storage;
    chunk.resize(number_of_threads);
    for(int i=0;i<number_of_threads;++i) {
      chunk[i] = 0.0;
    }

  } // index<user_output.GetSize()
}

void
Oxs_Ext::Fill__user_outputs
(const Oxs_SimState& /* state */,
 OC_INDEX node_start,
 OC_INDEX node_stop,
 int threadnumber)
{ // NOTE 1: This code assumes that the cells in the mesh
  //         are uniformly sized.
  // NOTE 2: This code assumes that Ms is fixed.
  for(OC_INDEX index=0;index<user_output.GetSize();++index) {
    OxsExtUserOutput& uo = user_output[index];
    Oxs_ChunkScalarOutput<Oxs_Ext>& output = uo.output;
    if(output.cache.state_id != 0) {
      continue;
    }
    const Oxs_MeshValue<ThreeVector>& source
      = (uo.source_cacheable
         ? uo.source_field->cache.value : uo.source_buffer);
    const Oxs_MeshValue<ThreeVector>& selector = uo.selector;

    OC_REAL8m sum=0.0;
    for(OC_INDEX i=node_start;i<node_stop;++i) {
      sum += (source[i]*selector[i]);
    }

    uo.chunk_storage[threadnumber] += sum;
    
  } // index<user_output.GetSize()
}

void
Oxs_Ext::Fill__user_outputs_fini
(const Oxs_SimState& state,int number_of_threads)
{
  for(OC_INDEX index=0;index<user_output.GetSize();++index) {
    OxsExtUserOutput& uo = user_output[index];
    Oxs_ChunkScalarOutput<Oxs_Ext>& output = uo.output;
    if(output.cache.state_id != 0) {
      continue;
    }
    std::vector<OC_REAL8m>& chunk = uo.chunk_storage;
    OC_REAL8m sum = chunk[0];
    for(int i=1;i<number_of_threads;++i) {
      sum += chunk[i];
    }
    output.cache.value=sum*uo.scaling;
    output.cache.state_id=state.Id();
  } // index<user_output.GetSize()
}

void
Oxs_Ext::Fill__user_outputs_shares
(std::vector<Oxs_BaseChunkScalarOutput*>& buddy_list)
{
  buddy_list.clear();
  for(OC_INDEX index=0;index<user_output.GetSize();++index) {
    OxsExtUserOutput& uo = user_output[index];
    buddy_list.push_back(&(uo.output));
  }
}
