import { expect, test } from '@playwright/test'

test('configures a season and records every plate type', async ({ page }) => {
  await page.goto('')
  const encodedPhoto = await page.evaluate(() => {
    const canvas = document.createElement('canvas')
    canvas.width = 20
    canvas.height = 20
    const context = canvas.getContext('2d')!
    context.fillStyle = 'tomato'
    context.fillRect(0, 0, 20, 20)
    return canvas.toDataURL('image/png').split(',')[1]
  })
  const photo = { name: 'plate.png', mimeType: 'image/png', buffer: Buffer.from(encodedPhoto, 'base64') }
  await page.getByRole('button', { name: /setup/i }).click()
  await page.getByLabel('Person name').fill('Alice')
  await page.getByRole('button', { name: 'Add' }).click()
  await page.getByLabel('Name', { exact: true }).fill('2026 Pasta Bowl')
  await page.getByRole('button', { name: 'Create season' }).click()

  for (const [heading, label, name] of [
    ['Pastas', 'New Pastas', 'Rigatoni'],
    ['Sauces', 'New Sauces', 'Alfredo'],
    ['Proteins', 'New Proteins', 'Meatballs'],
    ['Soups', 'New Soups', 'Zuppa Toscana'],
  ]) {
    const card = page.getByRole('article').filter({ has: page.getByRole('heading', { name: heading }) })
    await card.getByLabel(label).fill(name)
    await card.getByRole('button', { name: 'Add' }).click()
  }

  await page.getByRole('button', { name: /outings/i }).click()
  await page.getByLabel('Location (optional)').fill('Downtown')
  await page.getByRole('button', { name: 'Start twirling' }).click()
  await page.getByRole('button', { name: 'Add a plate' }).click()
  await page.getByRole('combobox', { name: 'Pasta' }).selectOption({ label: 'Rigatoni' })
  await page.getByRole('combobox', { name: 'Sauce' }).selectOption({ label: 'Alfredo' })
  await page.getByRole('combobox', { name: 'Protein' }).selectOption({ label: 'Meatballs' })
  await page.getByText('large', { exact: true }).click()
  await page.getByText('Taken home', { exact: true }).click()
  await page.locator('.modal input[type=file]').setInputFiles(photo)
  await expect(page.getByAltText('plate photo preview')).toBeVisible()
  await page.getByRole('button', { name: 'Save plate' }).click()
  await expect(page.getByText('Large Rigatoni · Alfredo · Meatballs')).toBeVisible()
  await expect(page.getByAltText('This plate')).toBeVisible()

  await page.getByRole('button', { name: 'Add a plate' }).click()
  await page.getByText('soup', { exact: true }).click()
  await page.getByRole('combobox', { name: 'Soup' }).selectOption({ label: 'Zuppa Toscana' })
  await page.getByText('Left behind', { exact: true }).click()
  await page.getByRole('button', { name: 'Save plate' }).click()

  await page.getByRole('button', { name: 'Add a plate' }).click()
  await page.getByText('salad', { exact: true }).click()
  await page.getByRole('button', { name: 'Save plate' }).click()
  await expect(page.getByText('3 plates')).toBeVisible()

  await page.reload()
  await expect(page.getByRole('button', { name: /outings/i })).toBeVisible()
  await page.getByRole('button', { name: /outings/i }).click()
  await expect(page.getByText('Downtown')).toBeVisible()
})

test('loads offline and exposes update state', async ({ page, context }) => {
  await page.goto('')
  await page.waitForFunction(() => 'serviceWorker' in navigator)
  await page.evaluate(() => navigator.serviceWorker.ready)
  await context.setOffline(true)
  await page.reload()
  await expect(page.getByText('Pasta Tracker')).toBeVisible()
  await expect(page.getByText('Offline')).toBeVisible()
  await page.getByRole('button', { name: /settings/i }).click()
  await page.getByRole('button', { name: 'Check for update' }).click()
  await expect(page.getByRole('button', { name: 'Go online to check' })).toBeVisible()
})
