<?php 
$x = xslt_create();
echo xslt_process($x, 'sample.xsl', 'sample.xml');
?>
