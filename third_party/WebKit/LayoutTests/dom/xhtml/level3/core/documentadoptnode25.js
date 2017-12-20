/*
Copyright Â© 2001-2004 World Wide Web Consortium,
(Massachusetts Institute of Technology, European Research Consortium
for Informatics and Mathematics, Keio University). All
Rights Reserved. This work is distributed under the W3CÂ® Software License [1] in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

[1] http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231
*/

   /**
    *  Gets URI that identifies the test.
    *  @return uri identifier of test
    */
function getTargetURI() {
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentadoptnode25";
   }

var docsLoaded = -1000000;
var builder = null;

//
//   This function is called by the testing framework before
//      running the test suite.
//
//   If there are no configuration exceptions, asynchronous
//        document loading is started.  Otherwise, the status
//        is set to complete and the exception is immediately
//        raised when entering the body of the test.
//
function setUpPage() {
   setUpPageStatus = 'running';
   try {
     //
     //   creates test document builder, may throw exception
     //
     builder = createConfiguredBuilder();

      docsLoaded = 0;

      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      docsLoaded += preload(docRef, "doc", "hc_staff");

       if (docsLoaded == 1) {
          setUpPageStatus = 'complete';
       }
    } catch(ex) {
        catchInitializationError(builder, ex);
        setUpPageStatus = 'complete';
    }
}

//
//   This method is called on the completion of
//      each asychronous load started in setUpTests.
//
//   When every synchronous loaded document has completed,
//      the page status is changed which allows the
//      body of the test to be executed.
function loadComplete() {
    if (++docsLoaded == 1) {
        setUpPageStatus = 'complete';
    }
}

/**
*
    Invoke the adoptNode method on a new document with a new Element of this
    Document as the source.  Verify if the node has been adopted correctly by checking the
    nodeName of the adopted Element.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-adoptNode
*/
function documentadoptnode25() {
   var success;
    if(checkInitialization(builder, "documentadoptnode25") != null) return;
    var doc;
      var newElem;
      var newDoc;
      var domImpl;
      var adoptedNode;
      var adoptedName;
      var adoptedNS;
      var docElem;
      var rootNS;
      var rootName;
      var nullDocType = null;

      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docElem = doc.documentElement;

      rootNS = docElem.namespaceURI;

      rootName = docElem.tagName;

      newElem = doc.createElementNS("http://www.w3.org/1999/xhtml","th");
      domImpl = doc.implementation;
newDoc = domImpl.createDocument(rootNS,rootName,nullDocType);
      adoptedNode = newDoc.adoptNode(newElem);

    if(

    (adoptedNode != null)

    ) {
    adoptedName = adoptedNode.nodeName;

      adoptedNS = adoptedNode.namespaceURI;

      assertEquals("documentadoptnode25_1","th",adoptedName);
       assertEquals("documentadoptnode25_2","http://www.w3.org/1999/xhtml",adoptedNS);

    }

}

function runTest() {
   documentadoptnode25();
}