import { useEffect, useMemo, useRef, useState } from 'react'
import type { DatabaseSnapshot } from '../types'

/**
 * The facts, drawn.
 *
 * Every binary ground fact `p(a,b)` is an edge from `a` to `b` labelled `p`;
 * unary facts hang a small satellite off their argument. Rules are left out on
 * purpose - a rule is not a relation between two atoms, and drawing its head as
 * if it were would be a lie about the database.
 *
 * The layout is a plain spring embedder run for a fixed number of ticks and
 * then stopped. A database this size settles in well under a second and a
 * permanently running simulation would keep the tab busy for no gain.
 */

interface Node {
  id: string
  x: number
  y: number
  vx: number
  vy: number
  degree: number
}

interface Edge {
  from: string
  to: string
  label: string
}

interface Props {
  database: DatabaseSnapshot
  onPickNode: (atom: string) => void
}

const TICKS = 260
const AREA = 1000

/**
 * The drawing is laid out in units of its own width, so a mark keeps one size
 * however wide the rail is. The viewBox takes the rail's aspect rather than
 * being square: this panel is tall and narrow, and a square fitted into it
 * would leave two thirds of the height empty.
 */
const VIEW = 1000
const MARGIN = 90
const SPAN = VIEW - MARGIN * 2

export function RelationGraph({ database, onPickNode }: Props) {
  const [hidden, setHidden] = useState<Set<string>>(() => new Set())
  const [lit, setLit] = useState<string | null>(null)

  // height over width of the drawing area, so the layout can fill it
  const [aspect, setAspect] = useState(1)
  const wrapRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    const element = wrapRef.current
    if (!element) return
    const observer = new ResizeObserver(([entry]) => {
      const { width, height } = entry.contentRect
      if (width > 0 && height > 0) {
        setAspect(clamp(height / width, 0.5, 3))
      }
    })
    observer.observe(element)
    return () => observer.disconnect()
  }, [])

  const relations = useMemo(() => {
    const names = new Set<string>()
    for (const clause of database.clauses) {
      if (!clause.isRule && (clause.arity === 1 || clause.arity === 2) && isGround(clause.text)) {
        names.add(clause.name)
      }
    }
    return [...names].sort()
  }, [database.clauses])

  const { nodes, edges } = useMemo(() => {
    const edges: Edge[] = []
    const degrees = new Map<string, number>()

    const bump = (id: string) => degrees.set(id, (degrees.get(id) ?? 0) + 1)

    for (const clause of database.clauses) {
      if (clause.isRule || hidden.has(clause.name) || !isGround(clause.text)) continue
      const args = argumentsOf(clause.text)
      if (clause.arity === 2 && args.length === 2) {
        edges.push({ from: args[0], to: args[1], label: clause.name })
        bump(args[0])
        bump(args[1])
      } else if (clause.arity === 1 && args.length === 1) {
        const tag = `${clause.name}·`
        edges.push({ from: args[0], to: tag, label: clause.name })
        bump(args[0])
        bump(tag)
      }
    }

    const ids = [...degrees.keys()]
    const nodes: Node[] = ids.map((id, index) => {
      // a deterministic ring start, so the same database always settles the
      // same way and the picture does not jump on every poll
      const angle = (index / Math.max(1, ids.length)) * Math.PI * 2
      const radius = AREA * 0.3
      return {
        id,
        x: AREA / 2 + Math.cos(angle) * radius,
        y: AREA / 2 + Math.sin(angle) * radius,
        vx: 0,
        vy: 0,
        degree: degrees.get(id) ?? 1,
      }
    })

    layout(nodes, edges, aspect)
    normalise(nodes)
    return { nodes, edges }
  }, [database.clauses, hidden, aspect])

  const byId = useMemo(() => new Map(nodes.map((node) => [node.id, node])), [nodes])

  if (relations.length === 0) {
    return (
      <div className="empty">
        <p className="empty__title">Nothing to draw yet.</p>
        <p className="empty__hint">
          This view draws ground facts: <code>p(a,b)</code> becomes an edge from <code>a</code> to{' '}
          <code>b</code>. Load a sample or assert some facts and they appear here. Rules are left
          out — a rule is not a relation between two atoms.
        </p>
      </div>
    )
  }

  return (
    <div className="graph" ref={wrapRef}>
      <svg
        viewBox={`0 0 ${VIEW} ${MARGIN * 2 + SPAN * aspect}`}
        preserveAspectRatio="xMidYMid meet"
        role="img"
        aria-label={`Relation graph of ${nodes.length} atoms and ${edges.length} facts`}
      >
        <g>
          {edges.map((edge, index) => {
            const from = byId.get(edge.from)
            const to = byId.get(edge.to)
            if (!from || !to) return null
            const isLit = lit !== null && (edge.from === lit || edge.to === lit)
            return (
              <g key={index}>
                <line
                  className={`graph__edge${isLit ? ' graph__edge--lit' : ''}`}
                  x1={from.x}
                  y1={from.y}
                  x2={to.x}
                  y2={to.y}
                />
                {isLit && (
                  <text
                    className="graph__edge-label"
                    x={(from.x + to.x) / 2}
                    y={(from.y + to.y) / 2 - 8}
                    textAnchor="middle"
                  >
                    {edge.label}
                  </text>
                )}
              </g>
            )
          })}
        </g>
        <g>
          {nodes.map((node) => (
            <g
              key={node.id}
              className={`graph__node${lit === node.id ? ' graph__node--lit' : ''}`}
              onMouseEnter={() => setLit(node.id)}
              onMouseLeave={() => setLit(null)}
              onClick={() => onPickNode(node.id.replace(/·$/, ''))}
              style={{ cursor: 'pointer' }}
            >
              <circle cx={node.x} cy={node.y} r={16 + Math.min(18, node.degree * 3.5)} />
              <text x={node.x} y={node.y + 52}>
                {node.id}
              </text>
            </g>
          ))}
        </g>
      </svg>

      <div className="graph__legend">
        {relations.map((name) => (
          <button
            key={name}
            data-on={!hidden.has(name)}
            onClick={() =>
              setHidden((previous) => {
                const next = new Set(previous)
                if (next.has(name)) next.delete(name)
                else next.add(name)
                return next
              })
            }
            title={`${hidden.has(name) ? 'Show' : 'Hide'} ${name} facts`}
          >
            <i />
            {name}
          </button>
        ))}
      </div>
    </div>
  )
}

// ------------------------------------------------------------------ layout

/**
 * A database is usually several unrelated islands - a family tree here, a
 * couple of stray facts there. Laying them out together does not work: one
 * spring simulation over the lot pushes the islands to the far corners, the
 * bounding box is then all margin, and the interesting cluster ends up a
 * thumbnail in one corner.
 *
 * So each connected component is settled on its own and the components are
 * packed into a grid afterwards, largest first. Every island gets room in
 * proportion to its size, and the picture fills the panel.
 */
function layout(nodes: Node[], edges: Edge[], aspect: number) {
  if (nodes.length === 0) return
  const components = split(nodes, edges)

  const placed = components.map((component) => {
    settle(component.nodes, component.edges)
    fit(component.nodes, 0, 0, 1, 1)
    return { nodes: component.nodes, size: Math.sqrt(component.nodes.length) }
  })

  placed.sort((a, b) => b.size - a.size)

  // the grid takes the shape of the panel: a tall rail wants a column of
  // islands, a wide one a row. The first cell holds the biggest island, so the
  // eye lands on the part of the database that actually has structure.
  const columns = Math.max(1, Math.round(Math.sqrt(placed.length / aspect)))
  const rows = Math.ceil(placed.length / columns)
  const cellWidth = 1 / columns
  const cellHeight = aspect / rows

  // an equal cell each, but a two-node island drawn as large as a ten-node one
  // reads as equally important, so small islands are drawn smaller in theirs
  const largest = placed[0]?.size ?? 1

  placed.forEach((component, index) => {
    const column = index % columns
    const row = Math.floor(index / columns)
    const weight = Math.max(0.4, component.size / largest)
    const width = cellWidth * 0.86 * weight
    const height = cellHeight * 0.86 * weight
    fit(
      component.nodes,
      column * cellWidth + (cellWidth - width) / 2,
      row * cellHeight + (cellHeight - height) / 2,
      width,
      height,
    )
  })
}

function clamp(value: number, low: number, high: number): number {
  return Math.min(high, Math.max(low, value))
}

/** one spring simulation over a single connected component */
function settle(nodes: Node[], edges: Edge[]) {
  if (nodes.length < 2) return
  const index = new Map(nodes.map((node, i) => [node.id, i]))
  const k = AREA / Math.sqrt(nodes.length)

  for (let tick = 0; tick < TICKS; tick++) {
    const cooling = 1 - tick / TICKS

    for (let i = 0; i < nodes.length; i++) {
      for (let j = i + 1; j < nodes.length; j++) {
        const a = nodes[i]
        const b = nodes[j]
        let dx = a.x - b.x
        let dy = a.y - b.y
        let distance = Math.hypot(dx, dy)
        if (distance < 0.01) {
          // deterministic nudge - two nodes exactly on top of each other have
          // no direction to separate along
          dx = ((i * 7 + 3) % 11) - 5
          dy = ((j * 5 + 1) % 9) - 4
          distance = Math.hypot(dx, dy) || 1
        }
        const force = (k * k) / distance
        a.vx += (dx / distance) * force
        a.vy += (dy / distance) * force
        b.vx -= (dx / distance) * force
        b.vy -= (dy / distance) * force
      }
    }

    for (const edge of edges) {
      const a = nodes[index.get(edge.from) ?? -1]
      const b = nodes[index.get(edge.to) ?? -1]
      if (!a || !b || a === b) continue
      const dx = a.x - b.x
      const dy = a.y - b.y
      const distance = Math.hypot(dx, dy) || 0.01
      const force = (distance * distance) / k
      a.vx -= (dx / distance) * force
      a.vy -= (dy / distance) * force
      b.vx += (dx / distance) * force
      b.vy += (dy / distance) * force
    }

    const limit = AREA * 0.1 * cooling + 1
    for (const node of nodes) {
      const speed = Math.hypot(node.vx, node.vy) || 1
      node.x += (node.vx / speed) * Math.min(speed, limit)
      node.y += (node.vy / speed) * Math.min(speed, limit)
      node.vx = 0
      node.vy = 0
    }
  }
}

/** the islands, each with the edges that belong to it */
function split(nodes: Node[], edges: Edge[]) {
  const neighbours = new Map<string, string[]>()
  for (const node of nodes) neighbours.set(node.id, [])
  for (const edge of edges) {
    neighbours.get(edge.from)?.push(edge.to)
    neighbours.get(edge.to)?.push(edge.from)
  }

  const byId = new Map(nodes.map((node) => [node.id, node]))
  const seen = new Set<string>()
  const components: { nodes: Node[]; edges: Edge[] }[] = []

  for (const node of nodes) {
    if (seen.has(node.id)) continue
    const members = new Set<string>()
    const queue = [node.id]
    seen.add(node.id)
    while (queue.length) {
      const id = queue.pop() as string
      members.add(id)
      for (const next of neighbours.get(id) ?? []) {
        if (!seen.has(next)) {
          seen.add(next)
          queue.push(next)
        }
      }
    }
    components.push({
      nodes: [...members].map((id) => byId.get(id) as Node),
      edges: edges.filter((edge) => members.has(edge.from)),
    })
  }

  return components
}

/**
 * Rescale a set of nodes into the box at (x, y).
 *
 * A single island fitted squarely into a tall rail would sit in a band across
 * the middle with the rest empty, so the shorter axis is allowed to stretch -
 * up to [STRETCH] times the uniform scale. Past that it stops looking like a
 * relation diagram and starts looking like a smeared one.
 */
const STRETCH = 1.8

function fit(nodes: Node[], x: number, y: number, width: number, height: number) {
  if (nodes.length === 0) return
  if (nodes.length === 1) {
    nodes[0].x = x + width / 2
    nodes[0].y = y + height / 2
    return
  }

  const xs = nodes.map((node) => node.x)
  const ys = nodes.map((node) => node.y)
  const minX = Math.min(...xs)
  const minY = Math.min(...ys)
  const spanX = Math.max(1e-6, Math.max(...xs) - minX)
  const spanY = Math.max(1e-6, Math.max(...ys) - minY)

  const uniform = Math.min(width / spanX, height / spanY)
  const scaleX = Math.min(width / spanX, uniform * STRETCH)
  const scaleY = Math.min(height / spanY, uniform * STRETCH)

  const offsetX = x + (width - spanX * scaleX) / 2
  const offsetY = y + (height - spanY * scaleY) / 2

  for (const node of nodes) {
    node.x = (node.x - minX) * scaleX + offsetX
    node.y = (node.y - minY) * scaleY + offsetY
  }
}

/** width-relative units become viewBox units; both axes take the same scale,
    so nothing is stretched */
function normalise(nodes: Node[]) {
  for (const node of nodes) {
    node.x = MARGIN + node.x * SPAN
    node.y = MARGIN + node.y * SPAN
  }
}

// ------------------------------------------------------------------ parsing

/** `father(fred,peter)` -> ['fred', 'peter'] */
function argumentsOf(text: string): string[] {
  const open = text.indexOf('(')
  if (open < 0 || !text.endsWith(')')) return []
  const inner = text.slice(open + 1, -1)

  const args: string[] = []
  let depth = 0
  let quoted = false
  let current = ''
  for (const c of inner) {
    if (quoted) {
      current += c
      if (c === "'") quoted = false
      continue
    }
    if (c === "'") {
      quoted = true
      current += c
      continue
    }
    if (c === '(' || c === '[') depth++
    if (c === ')' || c === ']') depth--
    if (c === ',' && depth === 0) {
      args.push(current.trim())
      current = ''
      continue
    }
    current += c
  }
  if (current.trim()) args.push(current.trim())
  return args
}

/** no variables - a fact about named things, which is what can be drawn */
function isGround(text: string): boolean {
  return !/(^|[(,[\s])[A-Z_]/.test(text)
}
