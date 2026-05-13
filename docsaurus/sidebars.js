// @ts-check

/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  tutorialSidebar: [
    {
      type: 'category',
      label: 'Getting Started',
      link: {
        type: 'doc',
        id: 'intro/overview',
      },
      items: [
        'intro/overview',
        'intro/why-zith',
        'intro/installation',
        'quickstart/quickstart',
      ],
    },
    {
      type: 'category',
      label: 'Language Guide',
      link: {
        type: 'doc',
        id: 'language/overview',
      },
      items: [
        'language/spec-core-topics',
        'language/syntax',
        'language/types',
        'language/variables',
        'language/functions',
        'language/control-flow',
        'language/ownership',
        'language/memory',
        'language/modules',
        'language/packs',
        'language/contexts',
        'language/generics',
        'language/concurrency',
        'language/errors',
      ],
    },
    {
      type: 'category',
      label: 'CLI Reference',
      link: {
        type: 'doc',
        id: 'cli/overview',
      },
      items: [
        'cli/build',
        'cli/run',
        'cli/check',
        'cli/fmt',
        'cli/new',
        'cli/clean',
        'cli/compile',
        'cli/repl',
        'cli/docs',
        'cli/flags',
      ],
    },
    {
      type: 'category',
      label: 'Project Configuration',
      link: {
        type: 'doc',
        id: 'project/overview',
      },
      items: [
        'project/overview',
      ],
    },
    {
      type: 'category',
      label: 'FAQ',
      link: {
        type: 'doc',
        id: 'faq/overview',
      },
      items: [
        'faq/philosophy',
        'faq/security',
        'faq/rust-comparison',
        'faq/use-cases',
      ],
    },
    {
      type: 'category',
      label: 'Advanced Topics',
      link: {
        type: 'doc',
        id: 'advanced/overview',
      },
      items: [
        'advanced/how-to-use',
        'advanced/unsafe',
        'advanced/traits',
        'advanced/macros',
        'advanced/metaprogramming',
        'advanced/raw-pointers',
        'advanced/data-structures',
        'advanced/generics-deep',
      ],
    },
    {
      type: 'category',
      label: 'Community',
      link: {
        type: 'doc',
        id: 'community/overview',
      },
      items: [
        'community/overview',
        'community/contributing',
        'community/code-of-conduct',
        'community/chat',
      ],
    },
  ],
};

module.exports = sidebars;