import { useRef } from 'react';
import { t, tPlural } from '../lib/i18n';

interface ScriptEditorProps {
  value: string;
  onChange: (text: string) => void;
  error?: string;
  onFormat: () => void;
}

export function ScriptEditor({ value, onChange, error, onFormat }: ScriptEditorProps) {
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const lineNumbersRef = useRef<HTMLDivElement>(null);

  // Sync line numbers scroll with textarea
  function handleScroll() {
    if (lineNumbersRef.current && textareaRef.current) {
      lineNumbersRef.current.scrollTop = textareaRef.current.scrollTop;
    }
  }

  // Compute line numbers
  const lineCount = value.split('\n').length;
  const lineNumbers = Array.from({ length: lineCount }, (_, i) => i + 1).join('\n');

  // Handle tab key
  function handleKeyDown(e: React.KeyboardEvent<HTMLTextAreaElement>) {
    if (e.key === 'Tab') {
      e.preventDefault();
      const ta = textareaRef.current!;
      const start = ta.selectionStart;
      const end = ta.selectionEnd;
      const newValue = value.substring(0, start) + '    ' + value.substring(end);
      onChange(newValue);
      // Restore cursor after state update
      requestAnimationFrame(() => {
        ta.selectionStart = ta.selectionEnd = start + 4;
      });
    }
  }

  function handleCopy() {
    navigator.clipboard.writeText(value).catch(() => {});
  }

  return (
    <div className="script-editor">
      <div className="script-toolbar">
        <button className="btn btn-secondary btn-sm" onClick={onFormat}>
          {t('SCRIPT_FORMAT')}
        </button>
        <button className="btn btn-ghost btn-sm" onClick={handleCopy}>
          {t('SCRIPT_COPY')}
        </button>
        {error && (
          <span style={{ fontSize: 12, color: 'var(--error)', marginLeft: 4 }}>
            {t('SCRIPT_PARSE_ERROR')}
          </span>
        )}
        {!error && value.trim() && (
          <span style={{ fontSize: 12, color: 'var(--success)', marginLeft: 4 }}>
            {t('SCRIPT_VALID_NPPS')}
          </span>
        )}
        <span style={{ marginLeft: 'auto', fontSize: 11, color: 'var(--text-muted)' }}>
          {tPlural('SCRIPT_LINE_COUNT', lineCount)}
        </span>
      </div>

      <div className="script-editor-wrap">
        <div
          className="line-numbers"
          ref={lineNumbersRef}
          aria-hidden="true"
        >
          {lineNumbers}
        </div>
        <textarea
          ref={textareaRef}
          className="script-textarea"
          value={value}
          onChange={e => onChange(e.target.value)}
          onKeyDown={handleKeyDown}
          onScroll={handleScroll}
          spellCheck={false}
          autoComplete="off"
          autoCorrect="off"
          autoCapitalize="off"
          placeholder={t('SCRIPT_PLACEHOLDER')}
        />
      </div>

      {error && (
        <div className="script-error-banner">
          <span>⚠</span>
          <span>{error}</span>
        </div>
      )}
    </div>
  );
}
