window.addEventListener('load', function() {
  // Initialize the editor
  window.editor = new toastui.Editor({
    el: document.querySelector('#editor'),
    height: '100%',
    initialEditType: 'markdown',
    previewStyle: 'vertical',
    hideModeSwitch: false,
    hideToolbar: false,
    usageStatistics: false
  });

  // Define all window functions
  window.togglePreview = function(show) {
    const previewEl = document.querySelector('.toastui-editor-md-preview');
    if (previewEl) {
      previewEl.style.display = show ? 'block' : 'none';
    }
  };

  window.showEditor = function() {
    document.getElementById('start-page').style.display = 'none';
    document.getElementById('editor').style.display = 'block';
  };

  window.showStartPage = function() {
    document.getElementById('start-page').style.display = 'flex';
    document.getElementById('editor').style.display = 'none';
  };

  window.updateRecentFiles = function(files) {
    const list = document.getElementById('recent-files-list');
    list.innerHTML = '';
    files.forEach(file => {
      const div = document.createElement('div');
      div.className = 'recent-file';
      div.textContent = file.name;
      div.onclick = () => window.webkit.messageHandlers.openFile.postMessage(file.path);
      list.appendChild(div);
    });
  };

  // Setup event listeners
  editor.on('change', () => {
    if (window.webkit && window.webkit.messageHandlers.contentChanged) {
      window.webkit.messageHandlers.contentChanged.postMessage('');
    }
  });

  // Let the native code know we're ready
  if (window.webkit && window.webkit.messageHandlers.editorInitialized) {
    window.webkit.messageHandlers.editorInitialized.postMessage('');
  }
});


window.togglePreview = function(show) {
    const editorContainer = document.querySelector('.toastui-editor-defaultUI');
    if (editorContainer) {
        if (show) {
            editorContainer.classList.remove('preview-hidden');
        } else {
            editorContainer.classList.add('preview-hidden');
        }
        // Refresh editor layout
        if (window.editor) {
            window.editor.focus();
        }
    }
};
