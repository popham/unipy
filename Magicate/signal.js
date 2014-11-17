mergeInto(LibraryManager.library, {
    Py_FatalError : function(msg) {
        console.log('Error: '+msg);
    }
});
