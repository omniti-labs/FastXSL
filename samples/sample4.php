<?php 
$ss = fastxsl_parse_stylesheet_file('sample.xsl');
$xd = fastxsl_parse_xml_file('sample.xml');
$res = fastxsl_transform($ss, $xd);
fastxsl_output($ss, $res);
?>
