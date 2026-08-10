/**
 * Drawn icons, one stroke weight, one grid.  Small enough a library would be
 * more weight than the whole UI.
 */

interface Props {
  size?: number
  className?: string
}

const base = (size: number) => ({
  width: size,
  height: size,
  viewBox: '0 0 16 16',
  fill: 'none' as const,
  stroke: 'currentColor',
  strokeWidth: 1.4,
  strokeLinecap: 'round' as const,
  strokeLinejoin: 'round' as const,
  'aria-hidden': true,
})

export const ChevronRight = ({ size = 12, className }: Props) => (
  <svg {...base(size)} className={className}>
    <path d="M6 3.5 10.5 8 6 12.5" />
  </svg>
)

export const Search = ({ size = 12, className }: Props) => (
  <svg {...base(size)} className={className}>
    <circle cx="7" cy="7" r="4.25" />
    <path d="m10.25 10.25 3 3" />
  </svg>
)

export const Trash = ({ size = 12, className }: Props) => (
  <svg {...base(size)} className={className}>
    <path d="M2.75 4.25h10.5M6.5 4.25V2.75h3v1.5M4.25 4.25l.6 8.25a.9.9 0 0 0 .9.75h4.5a.9.9 0 0 0 .9-.75l.6-8.25" />
  </svg>
)

export const Replay = ({ size = 12, className }: Props) => (
  <svg {...base(size)} className={className}>
    <path d="M13 8a5 5 0 1 1-1.6-3.65" />
    <path d="M13.25 2v3h-3" />
  </svg>
)

export const Alert = ({ size = 14, className }: Props) => (
  <svg {...base(size)} className={className}>
    <circle cx="8" cy="8" r="5.75" />
    <path d="M8 5v3.5" />
    <path d="M8 10.75h.01" />
  </svg>
)

export const Broom = ({ size = 12, className }: Props) => (
  <svg {...base(size)} className={className}>
    <path d="M9.5 2.5 13 6" />
    <path d="M10.25 5.25 5.5 10l-2 3.5 3.5-2 4.75-4.75" />
    <path d="m4.5 9 2.5 2.5" />
  </svg>
)

export const Wave = ({ size = 12, className }: Props) => (
  <svg {...base(size)} className={className}>
    <path d="M1.5 8h2.25L5.5 3.5 8 12.5l2-6.5 1.25 2h3.25" />
  </svg>
)
