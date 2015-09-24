### Local Modifications/Additions ########################################
sub broken_custom_address {
    # Code used to fill ADDRESS field at the bottom of each HTML page.
    # It relies on having a LaTeX label "sec:Credits" set, and sets
    # up a link to that point.  ($ref_files{$key} returns the page
    # holding the label.)  Also, the LaTeX source must set the Perl
    # variable, "$myTODAY", typically with a command like
    #
    #            \HTMLset{myTODAY}{\today}
    #
    local($key) = q/sec:Credits/;
    "<BR><I><A HREF=\"$ref_files{$key}#$key\">" .
	"OOMMF Documentation Team</A><BR>$myTODAY</I>";
}

sub custom_address {
    # At some time the above "broken_custom_address" routine failed
    # to return a value for $ref_files{$key}.  So now just hardcode
    # in the Credits page.  As before, the LaTeX source must set the
    # Perl variable, "$myTODAY", typically with a command like
    #
    #            \HTMLset{myTODAY}{\today}
    #
    local($key) = q/sec:Credits/;
    "<BR><I><A HREF=\"Credits.html#$key\">" .
	"OOMMF Documentation Team</A><BR>$myTODAY</I>";
}

sub my_get_contents_page {
    # This routine returns the HTML page holding the LaTeX label
    # "page:contents", to be used to set a link to be used as a Contents
    # page for the HTML collection.
    local($key) = q/page:contents/;
    "$ref_files{$key}" ;
}

sub top_navigation_panel {
    # This code is copied from the latex2html source, modified to put the
    # CUSTOM button first, and to setup a Contents link to a user specified
    # page, via the LaTeX "\label{page:contents}" command.  If this label
    # is not set, then no Contents button will be displayed.
    local($GLOSSARY,$LOI,$LOR);
    local($border) = (length($NAV_BORDER) ? "BORDER=\"$NAV_BORDER\"" : "");
    local($url) = ($LOCAL_ICONS ? "" : "$ICONSERVER/");
    local($loi_img) = 
	"<IMG ALIGN=\"BOTTOM\" ALT=\"Identifiers\" $border SRC=\"${url}identifiers_motif.gif\">";
    local($lor_img) = 
	"<IMG ALIGN=\"BOTTOM\" ALT=\"Refinements\" $border SRC=\"${url}refinements_motif.gif\">";

    $GLOSSARY = &add_special_link($glossary_visible_mark, $gls_file, $file)
	if ($styles_loaded{'lips'} && $INDEX_IN_NAVIGATION);

    ($LOI,$LOR) = (&add_special_link($loi_img, $HTCweb_loi_file, $file),
		   &add_special_link($lor_img, $HTCweb_lor_file, $file))
	if $INDEX_IN_NAVIGATION;

    # "My" contents page
    local($contents_img) = 
	"<IMG ALIGN=\"BOTTOM\" $border ALT=\"contents\" SRC=\"${url}contents.gif\">";
    local($contents_page) = &my_get_contents_page ;
    local($mycontents) = &add_special_link($contents_img,$contents_page,$file);


    # Navigation panel
    "<!--Navigation Panel-->" .

    # Now add a few buttons with a space between them
    "$CUSTOM_BUTTONS $NEXT $UP $PREVIOUS $mycontents $INDEX $GLOSSARY $LOI $LOR" .
    
    "\n<BR>" .		# Line break
	
    # If ``next'' section exists, add its title to the navigation panel
    ($NEXT_TITLE ? "\n<B> Next:</B> $NEXT_TITLE" : undef) . 
    
    # Similarly with the ``up'' title ...
    ($UP_TITLE ? "\n<B> Up:</B> $UP_TITLE" : undef) . 
 
    # ... and the ``previous'' title
    ($PREVIOUS_TITLE ? "\n<B> Previous:</B> $PREVIOUS_TITLE" : undef) .

    # These <BR>s separate it from the text body.
    "\n<BR><BR>"
}

sub bot_navigation_panel {
    # This is a stripped down navigation panel, based on
    # top_navigation_panel above.

    # "My" contents page
    local($border) = (length($NAV_BORDER) ? "BORDER=\"$NAV_BORDER\"" : "");
    local($contents_img) = "<IMG ALIGN=\"BOTTOM\" ALT=\"Contents\" $border SRC=\"${url}contents.gif\">";
    local($contents_page) = &my_get_contents_page ;
    local($mycontents) = &add_special_link($contents_img,$contents_page,$file);

    # Navigation panel
    "<HR>\n" . "<!--Navigation Panel-->" .
    "$CUSTOM_BUTTONS $NEXT $UP $PREVIOUS $mycontents $INDEX" .
    "\n<BR>"
}

sub Xget_next_argument {
    local($next, $br_id, $pat);
    if (!(s/$next_pair_pr_rx/$br_id=$1;$next=$2;$pat=$&;''/eo)
	&& (!(s/$next_pair_rx/$br_id=$1;$next=$2;$pat=$&;''/eo))) {
	print " *** Could not find argument for command ***\n";
	print "$_\n";
    };
    ($next, $br_id, $pat);
}

sub do_cmd_HTMLset {
    local($_) = @_;
    local($which,$value,$hash,$dummy);
    local($hash, $dummy) = &get_next_optional_argument;
    $which = &missing_braces
	unless ((s/$next_pair_rx/$which = $2;''/eo)
	    ||(s/$next_pair_pr_rx/$which = $2;''/eo));
    $value = &missing_braces
	unless ((s/$next_pair_rx/$value = $2;''/eo)
	    ||(s/$next_pair_pr_rx/$value = $2;''/eo));
    if ($hash) {
	local($tmp) = "\$hash";
	if (defined ${$tmp}) { eval "\$$tmp{$which} = \"$value\";" }
    } elsif ($which) { eval "\$$which = \$value;" }
    $_;
}

sub do_cmd_HTMLget {
  local($_) = @_;
  my($var) = Xget_next_argument();
  join('',eval("\${$var}"),$_);
}


1;	# This must be the last line
