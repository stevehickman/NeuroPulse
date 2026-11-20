/*
 * ParamInputs.tsx — the controls every parameter in the web app is edited with, so the same kind of value is
 * always edited the same way.
 *
 *   Bounded on both sides      slider + an editable value field + a magnifier button (fine changes).
 *   Bounded on one side or none  a typed field only. A range with no end has no track.
 *   A choice from a list        a select. A yes/no                    a checkbox.
 *
 * Which of the first two a value gets follows from its bounds (paramBounds.controlKind), and the bounds live in
 * common/npps/bounds.json and nowhere else (read through lib/paramBounds.ts). Every value has a typed field, because the displayed value is the field:
 * Apple's HIG and Material both pair a slider with an editable value, and a slider alone is a poor way to enter an
 * exact number. Nothing here clamps what is typed. A value outside its bounds is marked, and the validator, the
 * compiler and the hub stay the places it is refused.
 *
 * ParamGrid lays the controls of one block out in groups, so all the sliders sit together, all the typed fields
 * together, and so on, whatever order the block lists them in.
 */
import { Children, isValidElement, useState } from 'react';
import type { ReactElement, ReactNode } from 'react';
import { t } from '../../../../common/lib/i18n';
import {
  ParamBound,
  ControlKind,
  controlKind,
  needsMagnifier,
  fineRange,
  inBounds,
} from '../lib/paramBounds';

// ─── The value editor: slider (when both sides are bounded) + typed field ─────

interface RangeEditorProps {
  value: number | undefined;
  bound: ParamBound;
  unit?: string;
  onChange: (v: number | undefined) => void;
  /** An empty field means "no value" (onChange(undefined)) instead of being ignored. */
  optional?: boolean;
  placeholder?: string;
  /** Show only the typed field even when both sides are bounded (a frequency with presets). */
  fieldOnly?: boolean;
  /** For the label above, when this editor sits inside a labelled field. */
  ariaLabel?: string;
}

export function RangeEditor({ value, bound, unit, onChange, optional, placeholder, fieldOnly, ariaLabel }: RangeEditorProps) {
  const kind: ControlKind = fieldOnly ? 'field' : controlKind(bound);
  const [draft, setDraft] = useState<string | null>(null);
  const [fineCenter, setFineCenter] = useState<number | null>(null);

  const out = value !== undefined && !inBounds(value, bound);
  const lo = bound.min?.value;
  const hi = bound.max?.value;

  function commitText(text: string) {
    setDraft(text);
    if (text.trim() === '') {
      if (optional) onChange(undefined);
      return;
    }
    const n = Number(text);
    if (Number.isFinite(n)) onChange(n);
  }

  const field = (
    <input
      type="number"
      className={`form-input range-field${out ? ' is-out-of-range' : ''}`}
      aria-label={ariaLabel}
      aria-invalid={out || undefined}
      min={lo}
      max={hi}
      step={bound.step}
      placeholder={placeholder}
      value={draft ?? (value === undefined ? '' : String(value))}
      title={out ? t('WEB_PARAM_OUT_OF_RANGE', { 0: lo ?? '−∞', 1: hi ?? '∞' }) : undefined}
      onChange={e => commitText(e.target.value)}
      onBlur={() => setDraft(null)}
    />
  );

  if (kind === 'field') {
    return (
      <div className="range-editor range-editor--field">
        {field}
        {unit && <span className="range-unit">{unit}</span>}
      </div>
    );
  }

  // Slider. The magnifier narrows the track to a window around the value so that one pixel is one step.
  const fine = fineCenter !== null;
  const needed = needsMagnifier(bound);
  const current = value ?? lo!;
  let win = fine ? fineRange(fineCenter, bound) : { min: lo!, max: hi! };
  if (fine && (current < win.min || current > win.max)) win = fineRange(current, bound);

  return (
    <div className="range-editor-stack">
      <div className="range-editor">
        <input
          type="range"
          className="range-slider"
          aria-label={ariaLabel}
          min={win.min}
          max={win.max}
          step={bound.step}
          value={Math.min(Math.max(current, win.min), win.max)}
          onChange={e => {
            setDraft(null);
            onChange(Number(e.target.value));
          }}
        />
        <button
          type="button"
          className={`btn btn-ghost btn-sm range-magnifier${fine ? ' on' : ''}`}
          disabled={!needed}
          aria-pressed={fine}
          title={needed ? (fine ? t('WEB_PARAM_FINE_ON') : t('WEB_PARAM_FINE_OFF')) : t('WEB_PARAM_FINE_NOT_NEEDED')}
          onClick={() => setFineCenter(fine ? null : current)}
        >
          🔍
        </button>
        {field}
        {unit && <span className="range-unit">{unit}</span>}
      </div>
      <div className="range-ends">
        <span>{win.min}</span>
        <span>{win.max}</span>
      </div>
    </div>
  );
}

// ─── A labelled value ─────────────────────────────────────────────────────────

interface ParamInputBase {
  label: string;
  bound: ParamBound;
  unit?: string;
  placeholder?: string;
  /** Explanatory prose under the control. */
  help?: string;
}
/** A required value always has a number. An optional one can be empty, which is `undefined`. */
export type ParamInputProps = ParamInputBase & (
  | { optional?: false; value: number; onChange: (v: number) => void }
  | { optional: true; value: number | undefined; onChange: (v: number | undefined) => void }
);

export function ParamInput(props: ParamInputProps) {
  const { label, value, bound, unit, placeholder, help } = props;
  const text = unit ? t('WEB_MOD_LABEL_WITH_UNIT', { 0: label, 1: unit }) : label;
  return (
    <div className="param-field">
      <label className="param-label">{text}</label>
      <RangeEditor
        value={value}
        bound={bound}
        optional={props.optional}
        placeholder={placeholder}
        ariaLabel={text}
        onChange={v => (props.optional ? props.onChange(v) : v !== undefined && props.onChange(v))}
      />
      {help && <p className="param-help">{help}</p>}
    </div>
  );
}

// ─── Choices and switches ─────────────────────────────────────────────────────

export function SelectField({ label, value, options, onChange }: {
  label: string;
  value: string;
  options: { value: string; label: string }[];
  onChange: (v: string) => void;
}) {
  return (
    <div className="param-field">
      <label className="param-label">{label}</label>
      <select className="form-select param-select" value={value} onChange={e => onChange(e.target.value)}>
        {options.map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
      </select>
    </div>
  );
}

export function CheckboxField({ label, value, onChange }: {
  label: string; value: boolean; onChange: (v: boolean) => void;
}) {
  return (
    <div className="param-field" style={{ justifyContent: 'flex-end' }}>
      <label className="param-checkbox-row">
        <input type="checkbox" checked={value} onChange={e => onChange(e.target.checked)} />
        {label}
      </label>
    </div>
  );
}

// ─── Grouping ─────────────────────────────────────────────────────────────────

type Group = 'choice' | 'field' | 'slider' | 'toggle';

function groupOf(child: ReactNode): Group {
  if (!isValidElement(child)) return 'choice';
  const el = child as ReactElement<{ bound?: ParamBound; fieldOnly?: boolean }>;
  if (el.type === ParamInput) return controlKind(el.props.bound!) === 'slider' ? 'slider' : 'field';
  if (el.type === CheckboxField) return 'toggle';
  if (el.type === SelectField) return 'choice';
  // Anything that carries its own bound and is typed (a frequency with presets) sits with the typed fields.
  if (el.props && 'bound' in el.props) return 'field';
  return 'choice';
}

/** Lays a block's controls out by kind: choices, typed fields, sliders, switches. Order within a kind is the
 *  order the block lists them. */
export function ParamGrid({ children }: { children: ReactNode }) {
  const groups: Record<Group, ReactNode[]> = { choice: [], field: [], slider: [], toggle: [] };
  Children.toArray(children).forEach(c => groups[groupOf(c)].push(c));
  const order: Group[] = ['choice', 'field', 'slider', 'toggle'];
  return (
    <div className="param-groups">
      {order.map(g => groups[g].length > 0 && (
        <div key={g} className={`param-grid param-grid--${g}`}>{groups[g]}</div>
      ))}
    </div>
  );
}
