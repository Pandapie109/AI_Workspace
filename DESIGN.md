# M0 Design and Understanding Note

Answer briefly in your own words. This is not intended to be a long report.

1. What responsibility belongs to `Workspace`, and what responsibilities belong to `Document`, `Prompt`, and `Message` instead?

Workspace is responsible for grouping and managing the documents, prompts, and messages that belong to one work context. Document, Prompt, and Message are responsible for storing and managing their own individual data.

2. Why are the collections inside `Workspace` private? Explain the purpose of the const and non-const `At` overloads.

The collections are private so they cannot be changed directly in ways that bypass the Workspace interface. The non-const At methods allow a stored object to be modified, while the const versions allow it to be accessed without changing it.

3. Explain one meaningful test you added. What behavior does it check, and what implementation error could it catch?

One test I added checks copy independence. I copy a Workspace, change objects in the original, and verify that the copy stays unchanged. This could catch an implementation where two workspaces accidentally share the same underlying data.

4. Describe one implementation decision that you verified, tested, or revised before submitting your work.

I verified that Document::load() does not erase valid existing data when a file fails to load. I made sure the document is only updated after the file has been successfully opened and read.

5. If generative AI was used, disclose it as required by course policy. If no generative AI was used, state that. The disclosure itself is not used as proof of authorship or understanding.

I used ChatGPT to debug my code as I forgot to run it in a docker originially. I wasn't understanding the syntax error so I pasted it into chatGPT to which it told me it was a windows error not an error with my code. I then ran it in the linux docker and my code worked great.