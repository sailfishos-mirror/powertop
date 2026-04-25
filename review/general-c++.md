# C++ coding guidelines

These coding rules apply to all C++ code in the project.
PowerTOP uses the C++ 20 language standard


## Language standard

- [ ] Use only constructs in the C++ 20 standard that are non-deprecated or
    obsoloted.

- [ ] Undefined behavior (UB) language constructs are a violation of coding
    style


## STL

- [ ] Strongly prefer STL constructs over open-coded equivalent constructs

- [ ] Prefer std::string over C style "char *", except when interfacing with
    C class libraries

