// @ts-nocheck
// Note: // @ts-check not needed if you don't want type checking

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'Zith Programming Language',
  tagline: 'A modern systems programming language with clarity and safety',
  favicon: 'img/favicon.svg',

  onBrokenLinks: 'warn',
  onBrokenMarkdownLinks: 'warn',

  // Set the production url of your site here
  url: 'https://zith-lang.dev',
  // Set the /<baseUrl>/ pathname under which your site is served
  baseUrl: '/Zith',

  // GitHub pages deployment config.
  organizationName: 'galaxyhaze',
  projectName: 'Zith',
  trailingSlash: false,

  // See https://docusaurus.io/docs/api/docusaurus-config#deploymentconfig
  deploymentBranch: 'gh-pages',

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          sidebarPath: require.resolve('./sidebars.js'),
          routeBasePath: '/docs',
          editUrl: 'https://github.com/galaxyhaze/Zith/tree/main/docsaurus/',
        },
        blog: false,
        theme: {
          customCss: require.resolve('./src/css/custom.css'),
        },
      }),
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      navbar: {
        title: 'Zith',
        logo: {
          alt: 'Zith Logo',
          src: 'img/logo.svg',
        },
        items: [
{
            type: 'docSidebar',
            sidebarId: 'tutorialSidebar',
            position: 'left',
            label: 'Documentation',
          },
          {
            type: 'dropdown',
            position: 'left',
            label: 'Ecosystem',
            items: [
              {
                label: 'Zith Core',
                to: '/docs/intro/overview',
              },
              {
                label: 'Zith LSP',
                to: '#',
                disabled: true,
              },
              {
                label: 'Community',
                to: '/docs/community/overview',
              },
            ],
          },
          {
            href: '/docs/community/overview',
            label: 'Community',
            position: 'left',
          },
          {
            type: 'dropdown',
            position: 'left',
            label: 'Ecosystem',
            items: [
              {
                label: 'Zith Core',
                to: '/docs/intro/overview',
              },
              {
                label: 'Zith LSP',
                to: '#',
                disabled: true,
              },
              {
                label: 'Add Your Project',
                href: 'https://github.com/galaxyhaze/Zith-discussions',
              },
            ],
          },
          {
            href: 'https://github.com/galaxyhaze/Zith',
            label: 'GitHub',
            position: 'right',
          },
        ],
      },
      footer: {
        style: 'dark',
        links: [
          {
            title: 'Docs',
            items: [
              {
                label: 'Getting Started',
                to: '/docs/intro/overview',
              },
              {
                label: 'Language Guide',
                to: '/docs/language/overview',
              },
              {
                label: 'CLI Reference',
                to: '/docs/cli/overview',
              },
              {
                label: 'Advanced Topics',
                to: '/docs/advanced/overview',
              },
            ],
          },
          {
            title: 'Ecosystem',
            items: [
              {
                label: 'Zith Core',
                href: 'https://github.com/galaxyhaze/Zith',
              },
              {
                label: 'Zith LSP',
                to: '#',
                disabled: true,
              },
              {
                label: 'Add Your Project',
                href: 'https://github.com/galaxyhaze/Zith-discussions',
              },
            ],
          },
          {
            title: 'Community',
            items: [
              {
                label: 'Discord',
                href: 'https://discord.gg/585JgbWCPr',
              },
              {
                label: 'GitHub Discussions',
                href: 'https://github.com/galaxyhaze/Zith-discussions/discussions',
              },
              {
                label: 'Report a Bug',
                href: 'https://github.com/galaxyhaze/Zith/issues',
              },
              {
                label: 'Contributing',
                to: '/docs/community/contributing',
              },
            ],
          },
          {
            title: 'More',
            items: [
              {
                label: 'GitHub',
                href: 'https://github.com/galaxyhaze/Zith',
              },
            ],
          },
        ],
        copyright: `Copyright © ${new Date().getFullYear()} Zith Programming Language.`,
      },
      prism: {
        additionalLanguages: ['rust', 'bash', 'toml'],
      },
      colorMode: {
        defaultMode: 'dark',
        disableSwitch: true,
        respectPrefersColorScheme: false,
      },
    }),
};

module.exports = config;
