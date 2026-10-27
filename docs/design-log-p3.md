# Project 3 Design Log

**Author:** Daniel Latorre

**P2 code used:** My own

**Tools used:** I used OpenAI Codex to review the requirements, help draft parts of the implementation, add tests, and look for cases I might have missed. I also used Git, CMake, CTest, Apple Clang with warnings treated as errors, and UBSan to check the project.

## 1. Hash Table

I chose separate chaining instead of linear probing because I thought it would be easier to debug. Each bucket is a vector of `unique_ptr<Tool>`. If two names hash to the same index, they go into the same bucket. I can look through that bucket to see what collided or why a search failed. With linear probing, I'd have to keep track of where entries ended up as the table filled up.

I started with 16 buckets and used the 64 bit FNV 1a hash. It reads every byte in the tool's name and takes the order of the characters into account. I use modulo to turn the hash into a bucket index.

The table doubles before the next insertion would put the load factor above 0.75. Resizing means rehashing the existing tools, which takes O(n) time. It doesn't happen on every insertion, though. Since the table doubles in size, `add()` is O(1) amortized. Searching is O(1) expected as long as the names are spread out across the buckets.

I tested the table with 10,000 names. FNV 1a ended with 16,384 buckets, a load factor of 0.610352, and a longest chain of 6. For comparison, I used the weaker `s[0] + s.size()` hash with the same number of buckets. Its longest chain was 9,000.

That difference was much larger than I expected. Even with thousands of buckets, searching a chain with 9,000 entries would be slow. Making the table bigger doesn't do much if the hash keeps putting most of the names in the same bucket.

## 2. Pointer Stability

The registry also has to keep pointers from `find()` valid after a resize. I used `unique_ptr` so the registry owns each tool without storing the objects directly in the bucket vectors. When I rehash, I move the smart pointers into new buckets, but the actual `Tool` objects stay where they are in memory.

I checked this in my test by saving the pointer to the first tool, inserting the rest of the 10,000 tools, and checking that the pointer was still the same. I also tested duplicate names to make sure the extra tool gets destroyed and the original one stays in the registry. Without `unique_ptr`, I would have needed more code to handle deleting those objects myself.

## 3. Calculator Trace

For `2 * (3 + 4) - 5`, I used one stack for values and another for operators.

| Token / Action | Value Stack | Operator Stack | Description |
| --- | --- | --- | --- |
| Read `2` | `[2]` | `[]` | Push the number |
| Read `*` | `[2]` | `[*]` | Push multiplication |
| Read `(` | `[2]` | `[* (]` | Push left parenthesis |
| Read `3` | `[2, 3]` | `[* (]` | Push the number |
| Read `+` | `[2, 3]` | `[* ( +]` | Wait to add inside the parentheses |
| Read `4` | `[2, 3, 4]` | `[* ( +]` | Push the number |
| Read `)` | `[2, 7]` | `[*]` | Add `3 + 4` and remove `(` |
| Read `-` | `[14]` | `[-]` | Multiply `2 * 7`, then push subtraction |
| Read `5` | `[14, 5]` | `[-]` | Push the number |
| End | `[9]` | `[]` | Subtract `14 - 5` |

When the calculator reaches subtraction, it finishes the multiplication that is waiting on the operator stack. The parentheses also make sure `3 + 4` gets done first. After those steps, the last operation is `14 - 5`.

## 4. Errors as Values

For bad input, I return an error message instead of throwing an exception. For example, `Workspace::resolve` rejects `../etc/passwd` because it would leave the workspace. Missing files, invalid tool calls, and division by zero also return errors the harness can pass back to the model.

Since requests are coming from a language model, I can't assume that every path or expression will be valid. A bad request should give the model a chance to try something else instead of stopping the program.

I would still use exceptions for mistakes in the C++ code, like reading the wrong JSON type. The dispatcher catches exceptions as a backup.

## 5. What I Would Design Differently

If I could change one part of the provided interface, it would be `Tool::name()`. Right now it returns a `std::string` by value. When the table resizes, the registry calls it again for every tool, which can mean copying names it already has.

I'd either store the name with the registry entry or have `Tool::name()` return a `std::string_view` referring to the name stored in the tool. That would save some string copies when rehashing thousands of tools.
