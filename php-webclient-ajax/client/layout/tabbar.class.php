<?php
/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

?>
<?php
/**
 * This class can be used to output a tabbar control
 *
 * HOWTO BUILD:
 * - $tabs = array("general" => ("General"), "appointment" => _("Appointment")); //initialize your tab's
 * - $tabbar = new TabBar($tabs, key($tabs));
 * HOWTO USE:
 * - $tabbar->createTabs();        //this is the point where you like to place the tabbar
 * - $tabbar->beginTab("general"); //create the tab  
 * - //insert your code (php, html or javascript)
 * - $tabbar->endTab();            //close the tab 
 * DEPENDS ON:
 * |------> tabbar.js
 * |------> tabbar.css
 */

class Tabbar {
  var $id;
  var $tabs;
  var $selected;
 
  /** 
   * Constructor
   *
   * @param array  $tabs     This array contains the names and DOM id's for the tabs
   * @param mixed  $selected If this is a string, then the tab with this 'id' is marked selected, if this is an array, then it assumes that every entry must be marked selected
   * @param string $id       This is the id for the tabbar itself (for use in CSS) default is 'tabbar'
   *
   * Example $tabs array:
   *
   * $tabs = array('tab1'=>'Home','tab2'=>'Test');
   * Now we have 2 tabs, 'tab1' is the id for the element that contains the 'Home' tab etc. The name of the tabbutton will be 'tab_tab1'
   *
   * NOTE: please remember that when you use more then 1 tabbar on a page you must use different id's, also note that some modifications are needed in the CSS
   */
   
  function Tabbar($tabs,$selected = '',$id='tabbar'){
    $this->tabs = $tabs;
    $this->id   = $id;
    
    if (!is_array($selected)){ // making $selected always an array, just to keep the loop simple
      $selected = array($selected);
    }
    $this->selected = $selected;
  }

  /**
   * function to output init javascript code
   */
  function initJavascript($varName = "tabbar", $indenting = "\t"){
	$result = "";
    $result .= $indenting."var ".$varName."_tabpages = Array();\n";
    $num = 0;
    foreach($this->tabs as $id=>$title){
       $result .= $indenting.$varName."_tabpages[\"".$id."\"] = \"".$title."\";\n";
    }
    $result .= $indenting."var ".$varName." = new TabBar(\"".$this->id."\", ".$varName."_tabpages);\n";
	
	echo  $result;
  }

  /**
   * function which must be called just before any output what needs to be on a tabpage
   *
   * @param string $tab_id The ID for this tabpage
   */
  function beginTab($tab_id){
    $class = 'tabpage'; // this var can contain the CSS class for the selected tab
    // find if this is a selected tab
    foreach($this->selected as $default){
      if ($tab_id==$default){
        $class .= ' selectedtabpage"';
        break; // we can break out of this loop, because we found a match
      }
    }
    echo "<div id=\"".$tab_id."_tab\" class=\"".$class."\">\n";
  }

  /**
   * This functions must be called after all output for the tab specified in the beginTab function
   *
   * NOTE: There is no check to see if this function has been called before you do beginTab again, this can result in bad HTML
   */
  function endTab(){
    echo "</div>\n";
  }

  /**
   * Function to create the tabs buttons
   */ 
  function createTabs(){
    echo "<div id=\"".$this->id."\"><ul>\n";

    // loop through all tabs 
    foreach($this->tabs as $tab_id=>$tab_title){
      $class = ''; // this var can contain the CSS class for the selected tab

	  if ($tab_id == 'tracking') {
	  	$class .= ' tab_hide';
	  }
      
      // find if this is a selected tab
      foreach($this->selected as $default){
        if ($tab_id==$default){
          $class .= 'selectedtab';
          break; // we can break out of this inner loop, because we found a match
        }
      }

      //output the tab button
      echo "<li id=\"tab_",$tab_id,"\"",($class!=""?" class=\"".$class."\"":""),">";
      echo "<span>",$tab_title,"</span>";
      echo "</li>\n";
    }  
    echo "</ul></div><br class=\"tabbar_end\">";
  }
}
?>
