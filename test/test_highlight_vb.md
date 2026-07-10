# Visual Basic .NET Highlight Test

Modules, classes, LINQ, and Async/Await.

```vb
Imports System
Imports System.Collections.Generic
Imports System.Threading.Tasks

Module Program
    Sub Main()
        Dim service As New CalculatorService()
        Dim result = service.Add(10, 20)
        Console.WriteLine($"Result: {result}")

        Dim numbers = New List(Of Integer) From {1, 2, 3, 4, 5}
        Dim evens = numbers.Where(Function(n) n Mod 2 = 0).ToList()

        For Each n In evens
            Console.WriteLine(n)
        Next
    End Sub
End Module

Public Class CalculatorService
    Private ReadOnly _history As New List(Of String)

    Public Function Add(a As Integer, b As Integer) As Integer
        Dim result = a + b
        _history.Add($"{a} + {b} = {result}")
        Return result
    End Function

    Public Function Multiply(a As Integer, b As Integer) As Integer
        Return a * b
    End Function

    Public Async Function ProcessAsync(url As String) As Task(Of String)
        Using client As New HttpClient()
            Dim response = Await client.GetStringAsync(url)
            Return response
        End Using
    End Function

    Public ReadOnly Property Count As Integer
        Get
            Return _history.Count
        End Get
    End Property
End Class

Public Class User
    Public Property Id As Integer
    Public Property Name As String
    Public Property Email As String
    Public Property IsActive As Boolean

    Public Overrides Function ToString() As String
        Return $"{Name} <{Email}>"
    End Function
End Class
```
