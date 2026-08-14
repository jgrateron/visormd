# Go Highlight Test

Keywords, predeclared types, builtins, strings, runes, raw strings, comments, and numbers.

```go
package main

import (
    "fmt"
    "strings"
)

// Point represents a 2D coordinate.
type Point struct {
    X, Y float64
    name string
}

// Distance returns the euclidean distance to another point.
func (p Point) Distance(q Point) float64 {
    dx := p.X - q.X
    dy := p.Y - q.Y
    return math.Sqrt(dx*dx + dy*dy)
}

type Shape interface {
    Area() float64
    Perimeter() float64
}

const (
    StatusOK = iota
    StatusError
)

var (
    count int     = 42
    ratio float64 = 3.14
    name  string  = "VisorMD"
)

func main() {
    /* Multi-line comment
       spanning two lines */
    p := Point{X: 1.0, Y: 2.0}
    q := Point{X: 4.0, Y: 6.0}
    fmt.Println(p.Distance(q))

    ch := make(chan int, 1)
    go func() {
        ch <- 42
        defer close(ch)
    }()

    select {
    case v := <-ch:
        fmt.Println(v)
    default:
        fmt.Println("no value")
    }

    for i := 0; i < 10; i++ {
        switch {
        case i%2 == 0:
            continue
        case i > 7:
            break
        }
    }

    names := []string{"alpha", "beta"}
    for _, n := range names {
        fmt.Println(strings.ToUpper(n))
    }

    if err := validate(p); err != nil {
        panic(err)
    }

    var raw string = `C:\Users\go\path`
    letter := 'A'
    tab := '\t'

    values := map[string]any{
        "hex":   0xFF,
        "bin":   0b1010,
        "oct":   0o755,
        "sep":   1_000_000,
        "real":  3.5e-2,
        "imag":  1i,
        "ratio": ratio,
    }
    fmt.Println(len(values), cap(values))

    total := sum(1, 2, 3)
    print(total)
}

func sum(nums ...int) int {
    total := 0
    for _, n := range nums {
        total += n
    }
    return total
}
```
