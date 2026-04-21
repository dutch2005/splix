```markdown
# splix Development Patterns

> Auto-generated skill from repository analysis

## Overview
This skill teaches you the core development patterns and conventions used in the `splix` TypeScript codebase. You'll learn about file naming, import/export styles, commit message conventions, and how to write and run tests. This guide is ideal for contributors or anyone looking to maintain consistency within the `splix` project.

## Coding Conventions

### File Naming
- **PascalCase** is used for file names.
  - Example: `MyComponent.ts`, `Utils.ts`

### Import Style
- **Relative imports** are used for internal modules.
  - Example:
    ```typescript
    import { MyFunction } from './Utils';
    ```

### Export Style
- **Named exports** are preferred.
  - Example:
    ```typescript
    export function doSomething() { /* ... */ }
    ```

### Commit Messages
- **Conventional commit format** is used.
- Common prefix: `chore`
- Example:
  ```
  chore: update dependencies and fix minor bugs
  ```

## Workflows

### Making a Change
**Trigger:** When you want to add a feature or fix a bug  
**Command:** `/make-change`

1. Create a new branch for your change.
2. Follow PascalCase for new file names.
3. Use relative imports for any internal modules.
4. Use named exports for all exported functions or variables.
5. Write or update tests in files matching `*.test.*`.
6. Commit using the conventional format (e.g., `chore: describe your change`).
7. Open a pull request for review.

### Writing Tests
**Trigger:** When you add new code or modify existing code  
**Command:** `/write-test`

1. Create or update a test file with the pattern `*.test.*` (e.g., `MyComponent.test.ts`).
2. Write tests for your code changes.
3. Run the tests using the project's test runner (framework unknown; check project scripts or documentation).
4. Ensure all tests pass before committing.

## Testing Patterns

- Test files follow the `*.test.*` naming pattern, such as `MyComponent.test.ts`.
- The specific testing framework is not detected; refer to project documentation or scripts for running tests.
- Example test file:
  ```typescript
  // MyComponent.test.ts
  import { myFunction } from './MyComponent';

  describe('myFunction', () => {
    it('should work as expected', () => {
      expect(myFunction()).toBe(true);
    });
  });
  ```

## Commands
| Command        | Purpose                                      |
|----------------|----------------------------------------------|
| /make-change   | Guide for making a code change or feature    |
| /write-test    | Steps for writing and running tests          |
```
