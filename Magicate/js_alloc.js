mergeInto(LibraryManager.library, {
    js_alloc : function(length) {
        return new Uint8Array(length);
    },
});
