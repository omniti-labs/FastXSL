<?php
define('FASTXSL_NOCACHE_RESULT', 0);
define('FASTXSL_PRMCACHE_RESULT', 1);
define('FASTXSL_SHMCACHE_RESULT', 2);

/**
 * The base class for which documents are managed, the FastXSL_Cache and FastXSL_NoCache classes
 * both inherit from this class.
 *
 * This class is not directly instiable, instead use either FastXSL_Cache or FastXSL_NoCache.
 *
 * @access public
 * @author Sterling Hughes <sterling@php.net>
 * @since  FastXSL 1.0
 *
 */
class FastXSL_Base {
	/**
	 * Whether or not the files passed to FastXSL will be absolute paths.  If this 
	 * variable is set to false, then a realpath() will be done on every file to 
	 * assure that it is being accessed through an absolute path.
	 * 
	 * @access public
	 * @since  FastXSL 1.0
	 */
	var $useAbsolutePath = false;

	/**
	 * Whether or not to stat() the documents in the fastxsl cache.  If this is turned off, 
	 * then documents will not be checked for freshness, and you need to gracefully restart
	 * Apache in order for new documents to be recognized.
	 *
	 * @access public
	 * @since  FastXSL 1.0
	 */
	var $noStat = false;

	function FastXSL_Base() {
	}

	/**
	 * Resolves a relative path to an absolute path using the realpath() system call which not
	 * only expands paths, but properly handles symlinks and the like.  This function respects
	 * the $useAbsolutePath property, and will avoid the expensive realpath() call if 
	 * $useAbsolutePath is off.
	 *
	 * @param string The filename to resolve
	 * @param string The class and method name pair for error reporting
	 * 
	 * @return boolean 
	 *
	 * @access private
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  FastXSL 1.0
	 */
	function resolvePath(&$filename, $class_meth_name) {
		if ($this->useAbsolutePath == false) {
			$resolved_filename = realpath($filename);
			if ($resolved_filename === false) {
				trigger_error(E_WARNING, "Cannot resolve path of XML file $filename passed to $class_meth_name");
				return false;
			}
			$filename = $resolved_filename;
		}
		
		return true;
	}

	/**
	 * Parses an XML file into a FastXSL XML resource for use with the transform() or profile() methods.
	 * 
	 * @param string The name of the file you'd like to parse
	 *
	 * @return resource the FastXSL XML document
	 * 
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  FastXSL 1.0
	 */
	function parseXMLFile($filename) {
		$this->resolvePath($filename, "FastXSL::setParseXMLFile()");
		return fastxsl_xml_parsefile($filename);
	}

	/**
	 * Parses XML data into a FastXSL XML resource for use with the transform() or profile() methods.
	 *
	 * @param string The XML data to compile
	 *
	 * @return resource The FastXSL XML Document
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  FastXSL 1.0
	 */
	function parseXMLData($data) {
		return fastxsl_xml_parsestring($data);
	}

	/**
	 * Parses an XSL stylesheet into a FastXSL Stylesheet resource for use with the transformParsed() or profile()
	 * methods.
	 *
	 * @param string The stylesheet file to compile
	 *
	 * @return resource The FastXSL stylesheet document
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  FastXSL 1.0
	 */
	function parseStylesheetFile($stylesheet_file) {
		$this->resolvePath($stylesheet_file, "FastXSL::parseStylesheetFile()");
		return fastxsl_stylesheet_parsefile($stylesheet_file);
	}
	
	function &transformParsed($stylesheet, $xml, $parameters = null) 
	{
		$res = fastxsl_nocache_transform($stylesheet, $xml, $parameters);
		if (!$res) {
			return false;
		}

		$resobj = &new FastXSL_Result(FASTXSL_NOCACHE_RESULT, $stylesheet, $res);
		return $resobj;
	}

	/**
	 * Profile the transformation of an XML document against a XSL stylesheet.
	 *
	 * @param resource The Stylesheet Document to profile
	 * @param resource The XML Document to profile
	 * @param array    Transformation resources 
	 * @param string   The directory to place the profile results
	 *
	 * @return mixed   A FastXSL result object on success, false on failure
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  FastXSL 1.0
	 */
	function &profile($stylesheet, $xml, $parameters, $profiledir) {
		$res = fastxsl_nocache_profile($stylesheet, $xml, $parameters, $filename);
		if (!$res) {
			return false;
		}

		$resobj = &new FastXSL_Result(FASTXSL_NOCACHE_RESULT, $stylesheet, $res);
		return $resobj;
	}
}

/**
 * A container that holds the result of an XSL transformation and provides a set of 
 * common methods for acting on that container.
 *
 * This class is instantiated by the transformation methods and should not be directly
 * instantiated.
 * 
 * @access private
 * @author Sterling Hughes <sterling@php.net>
 * @since  FastXSL 1.0
 */
class FastXSL_Result {
	var $type;
	var $stylesheet;
	var $result;

	/** 
	 * Constructor sets up the context for this object.
	 * 
	 * @param mixed The stylesheet associated with the resulting XML document
	 * @param resource The resulting XML document from a FastXSL transformation
	 * 
	 * @access private
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  FastXSL 1.0
	 */
	function FastXSL_Result($type, $stylesheet, $result) {
		$this->type = $type;
		$this->stylesheet = $stylesheet;
		$this->result = $result;
	}

	/**
	 * Return the value of this result object as a string
	 *
	 * @return string The string value of this XML document object
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  FastXSL 1.0
	 */
	function toString() {
		switch ($this->type) {
		case FASTXSL_NOCACHE_RESULT:
			return fastxsl_nocache_tostring($this->stylesheet, $this->result);
		case FASTXSL_PRMCACHE_RESULT:
			return fastxsl_prmcache_tostring($this->stylesheet, $this->result);
		case FASTXSL_SHMCACHE_RESULT:
			return fastxsl_shmcache_tostring($this->stylesheet, $this->result);
		}
	}
}

/**
 * FastXSL_Cache extends FastXSL_Base to provide shared memory cached transformations
 * to Apache processes.  This class calls the underlying fastxsl api's to transparently 
 * markup the transformations.
 *
 * @access public
 * @author Sterling Hughes <sterling@php.net>
 * @since  FastXSL 1.0
 */
class FastXSL_ShmCache extends FastXSL_Base {
	/**
	 * Constructor - calls the FastXSL_Base constructor.
	 * 
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  FastXSL 1.0
	 */
	function FastXSL_ShmCache() {
		$this->FastXSL_Base();
	}

	/**
	 * Transform an XSL stylesheet from the shared memory cache against an 
	 * XML document with the given parameters.  If the passed stylesheet does not 
	 * exist in shared memory, parse the stylesheet and place it in shared memory.
	 *
	 * @param string The filename of the sylesheet
	 * @param resource The XML document to parse
	 * @param array The parameters for the transformation
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  FastXSL 1.0
	 */
	function &transform($stylesheet, $xml, $parameters = null) {
		$this->resolvePath($stylesheet, "FastXSL_Cache::transform()");

		$res = fastxsl_shmcache_transform($stylesheet, $xml, $parameters);
		if (!$res) {
			return false;
		}

		$resobj = &new FastXSL_Result(FASTXSL_SHMCACHE_RESULT, $stylesheet, $res);
		return $resobj;
	}
}

/**
 * FastXSL_PrmCache extends FastXSL_Base to provide memory cached transformations
 * to Apache processes.  This class calls the underlying fastxsl api's to transparently 
 * markup the transformations.
 *
 * @access public
 * @author Sterling Hughes <sterling@php.net>
 * @since  FastXSL 1.0
 */
class FastXSL_PrmCache extends FastXSL_Base {
	/**
	 * Constructor - calls the FastXSL_Base constructor.
	 * 
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  FastXSL 1.0
	 */
	function FastXSL_PrmCache() {
		$this->FastXSL_Base();
	}

	/**
	 * Transform an XSL stylesheet from the memory cache against an 
	 * XML document with the given parameters.  If the passed stylesheet does not 
	 * exist in shared memory, parse the stylesheet and place it in shared memory.
	 *
	 * @param string The filename of the sylesheet
	 * @param resource The XML document to parse
	 * @param array The parameters for the transformation
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  FastXSL 1.0
	 */
	function &transform($stylesheet, $xml, $parameters = null) {
		$this->resolvePath($stylesheet, "FastXSL_Cache::transform()");

		$res = fastxsl_prmcache_transform($stylesheet, $xml, $parameters);
		if (!$res) {
			return false;
		}

		$resobj = &new FastXSL_Result(FASTXSL_PRMCACHE_RESULT, $stylesheet, $res);
		return $resobj;
	}
}


/**
 * This class implements a version of FastXSL that does not cache the documents in 
 * shared memory.  This class can directly replace the FastXSL_Cache class, and is 
 * useful for isolating cases where the shared memory class FastXSL_Cache, may be causing
 * problems.
 * 
 * Method signatures are the same as those of the FastXSL_Cache class.
 *
 * @access public
 * @author Sterling Hughes <sterling@php.net>
 * @since  FastXSL 1.0
 *
 */
class FastXSL_NoCache extends FastXSL_Base {
	function FastXSL() {
		$this->FastXSL_Base();
	}

	function &transform($stylesheet_file, $xml, $parameters = null) {
		return $this->transformParsed($this->parseStylesheetFile($stylesheet_file), $xml, $parameters);
	}
}


?>
