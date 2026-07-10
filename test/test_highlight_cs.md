# C# Highlight Test

Async/await, LINQ, generics, properties, records, and pattern matching.

```cs
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.EntityFrameworkCore;

namespace MyApp.Data;

public record Address(string Street, string City, string Country);

public class UserService : IDisposable
{
    private readonly AppDbContext _db;
    private readonly ILogger<UserService> _logger;

    public UserService(AppDbContext db, ILogger<UserService> logger)
    {
        _db = db ?? throw new ArgumentNullException(nameof(db));
        _logger = logger;
    }

    public async Task<User?> GetByIdAsync(Guid id)
    {
        return await _db.Users
            .Include(u => u.Address)
            .FirstOrDefaultAsync(u => u.Id == id);
    }

    public async Task<List<UserDto>> SearchAsync(
        string? query, int limit = 10)
    {
        var results = await _db.Users
            .Where(u => EF.Functions.Like(u.Name, $"%{query}%"))
            .OrderBy(u => u.Name)
            .Take(limit)
            .Select(u => new UserDto(
                u.Id, u.Name, u.Email, u.IsActive))
            .ToListAsync();

        _logger.LogInformation("Found {Count} users for '{Query}'",
            results.Count, query);
        return results;
    }

    public async Task SaveAsync(User user)
    {
        var entry = _db.Users.Update(user);
        await _db.SaveChangesAsync();
    }

    public void Dispose() => _db?.Dispose();
}

// Pattern matching and tuples
public static class Extensions
{
    public static string Describe(object obj) => obj switch
    {
        null           => "Nothing",
        int n and > 0  => $"Positive {n}",
        int n and < 0  => $"Negative {n}",
        string { Length: 0 } => "Empty string",
        string s       => $"String: {s}",
        DateTime d     => $"Date: {d:yyyy-MM-dd}",
        _              => "Unknown type"
    };

    public static (int min, int max, double avg) Stats(
        IEnumerable<int> numbers)
    {
        var list = numbers.ToList();
        return (list.Min(), list.Max(), list.Average());
    }
}

// Decimal, var, dynamic
decimal price = 19.99m;
var name = "Alice";
dynamic bag = new System.Dynamic.ExpandoObject();
bag.Count = 42;

// Null-coalescing and conditional access
string? maybe = null;
string result = maybe ?? "default";
int? length = maybe?.Length;
```
