# Beating `std::unordered_map`

In this repo, I aim at outperforming the standard library hash map.
The implementation is motivated by my recent project for _Database Systems Implementation_ course, where `std::unordered_map` proved to be a big bottleneck of the system.

We are going to base the implementation on the following 2 requirements inspired by the aforementioned use-case

- Pointer stability
- Deletes are very rare
