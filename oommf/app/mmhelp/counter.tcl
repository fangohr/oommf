# FILE: counter.tcl

Oc_Class Counter {

  public variable 	type = integer {
    if {($type != "integer") && ($type != "roman") && ($type != "alphabet") \
        && ($type != "list")} {
      error "legal values: alphabet integer list roman"
    }
  }

  public variable 	initial	= 1 {
    if {![regexp {^[0-9]+$} $initial] || ($initial == 0)} {
      error "value must be a positive integer"
    }
  }

  public variable 	increment = 1 {
    if {![regexp {^[0-9]+$} $increment] || ($increment == 0)} {
      error "value must be a positive integer"
    }
  }

  public variable 	case = upper {
    if {($case != "upper") && ($case != "lower")} {
      error "legal values: upper lower"
    }
  }

  public variable	list = {}

  variable value

  Constructor {args} {
    eval $this Configure $args
    $this Reset
  }

  method Reset {} {value initial} {
    set value $initial
    return [$this Get]
  }

  method Next {} {value increment} {
    set retval [$this Get]
    incr value $increment
    return $retval
  }

  private method Get {} {type value case list} {
    switch -- $type {
      alphabet {
        switch -- $case {
          upper {
            return [string toupper [$class IntegerToAlphabet $value]]
          }
          lower {
            return [$class IntegerToAlphabet $value]
          }
        }
      }
      integer {
        return $value
      }
      list {
        return [lindex $list [expr {$value % [llength $list]}]]
      }
      roman {
        switch -- $case {
          upper	{
            return [string toupper [$class IntegerToRoman $value]]
          }
          lower {
            return [$class IntegerToRoman $value]
          }
        }
      }
    }
  }

  # Convert an integer to its Roman numeral representation.
  proc IntegerToRoman {avalue} {} {
    if {![regexp {^[0-9]+$} $avalue] || ($avalue == 0)} {
      error "
'$class IntegerToRoman' error:
  	Invalid value: $avalue

usage:
	$class IntegerToRoman <value>

Invalid call:
	$class IntegerToRoman $avalue

<value> must be a positive integer"
    }
    if {$avalue > 3999} {
      error "
'$class IntegerToRoman' error:
  	Invalid value: $avalue

usage:
	$class IntegerToRoman <value>

Invalid call:
	$class IntegerToRoman $avalue

<value> exceeds range translatable to Roman numerals"
    }
    set result ""
    # Handle any thousands
    if {[string length $avalue] == 4} {
      for {set i 0} {$i < [string index $avalue 0]} {incr i} {
        append result m
      }
      set avalue [expr $avalue - $i * 1000]
    }
    set elements "mdclxvi"
    # Handle the rest
    for {set digits 3} {$digits > 0} {incr digits -1} {
      if {$digits == [string length $avalue]} {
        append result [$class RomanDigitMap [string index $avalue 0] \
            [string range $elements [expr 6 - 2 * $digits] \
            [expr 8 - 2 * $digits] ] ]
        regsub {^[0-9]} $avalue {} avalue
      }
    }
    return $result
  }

  # Convert a positive number value to an alphabetic representation.
  # Use a-z as "digits" of base 26 system, sort of. (There is no zero).
  # Assumes a-z contiguous in character set, like ASCII
  proc IntegerToAlphabet {avalue} {} {
    if {![regexp {^[0-9]+$} $avalue] || ($avalue == 0)} {
      error "
'$class IntegerToAlphabet' error:
  	Invalid value: $avalue

usage:
	$class IntegerToAlphabet <value>

Invalid call:
	$class IntegerToAlphabet $avalue

<value> must be a positive integer"
    }
    set result {}
    incr avalue -1
    if {$avalue > 25} {
      set l [expr $avalue / 26]
      set result [$class IntegerToAlphabet $l]
      set avalue [expr $avalue % 26]
    }
    append result [format %c [expr $avalue + \
		[set v 0; scan a %c v; set v]]]
    return $result
  }

  # Convert a decimal digit to a Roman numeral representation,
  # using the symbols in the 3-character string symbols as
  # the 10, 5, and 1 representer.
  private proc RomanDigitMap {digit symbols} {} {
    set i [string index $symbols 2]
    set v [string index $symbols 1]
    set x [string index $symbols 0]
    switch $digit {
      0	{return ""}
      1	{return $i}
      2	{return $i$i}
      3	{return $i$i$i}
      4	{return $i$v}
      5	{return $v}
      6	{return $v$i}
      7	{return $v$i$i}
      8	{return $v$i$i$i}
      9	{return $i$x}
    }
  }
}
